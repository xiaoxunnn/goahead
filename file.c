/*
    file.c -- File handler
  
    This module serves static file documents
 */

/********************************* Includes ***********************************/

#include    "goahead.h"

/*********************************** Locals ***********************************/

static char   *websIndex;                   /* Default page name */
static char   *websDocuments;               /* Default Web page directory */

/**************************** Forward Declarations ****************************/

static void fileWriteEvent(Webs *wp);

/*********************************** Code *************************************/
/*
    Serve static files
 */
static bool fileHandler(Webs *wp)
{
    WebsFileInfo    info;
    char            *tmp, *date;
    ssize           nchars;
    int             code;

    assure(websValid(wp));
    assure(wp->method);
    assure(wp->filename && wp->filename[0]);

#if !BIT_ROM
    if (smatch(wp->method, "DELETE")) {
        if (unlink(wp->filename) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Can't delete the URI");
        } else {
            /* No content */
            websResponse(wp, 204, 0);
        }
    } else if (smatch(wp->method, "PUT")) {
        /* Code is already set for us by processContent() */
        websResponse(wp, wp->code, 0);

    } else 
#endif /* !BIT_ROM */
    {
        /*
            If the file is a directory, redirect using the nominated default page
         */
        if (websPageIsDirectory(wp)) {
            nchars = strlen(wp->path);
            if (wp->path[nchars - 1] == '/' || wp->path[nchars - 1] == '\\') {
                wp->path[--nchars] = '\0';
            }
            tmp = sfmt("%s/%s", wp->path, websIndex);
            websRedirect(wp, tmp);
            gfree(tmp);
            return 1;
        }
        if (websPageOpen(wp, O_RDONLY | O_BINARY, 0666) < 0) {
#if BIT_DEBUG
            if (wp->referrer) {
                trace(1, "From %s\n", wp->referrer);
            }
#endif
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot open document for: %s", wp->path);
            return 1;
        }
        if (websPageStat(wp, &info) < 0) {
            websError(wp, HTTP_CODE_NOT_FOUND, "Cannot stat page for URL");
            return 1;
        }
        code = 200;
        if (info.mtime <= wp->since) {
            code = 304;
        }
        websSetStatus(wp, code);
        websWriteHeaders(wp, info.size, 0);
        if ((date = websGetDateString(&info)) != NULL) {
            websWriteHeader(wp, "Last-modified", "%s", date);
            gfree(date);
        }
        websWriteEndHeaders(wp);

        /*
            All done if the browser did a HEAD request
         */
        if (smatch(wp->method, "HEAD")) {
            websDone(wp);
            return 1;
        }
        websSetBackgroundWriter(wp, fileWriteEvent);
    }
    return 1;
}


/*
    Do output back to the browser in the background. This is a socket write handler.
    This bypasses the output buffer and writes directly to the socket.
 */
static void fileWriteEvent(Webs *wp)
{
    char    *buf;
    ssize   len, wrote;

    assure(wp);
    assure(websValid(wp));

    /*
        Note: websWriteSocket may return less than we wanted. It will return -1 on a socket error.
     */
    if ((buf = walloc(BIT_LIMIT_BUFFER)) == NULL) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR, "Can't get memory");
        return;
    }
    while ((len = websPageReadData(wp, buf, BIT_LIMIT_BUFFER)) > 0) {
        if ((wrote = websWriteSocket(wp, buf, len)) < 0) {
            break;
        }
        if (wrote != len) {
            websPageSeek(wp, - (len - wrote), SEEK_CUR);
            break;
        }
    }
    gfree(buf);
    if (len <= 0) {
        websDone(wp);
    }
}


PUBLIC int websProcessPutData(Webs *wp)
{
    ssize   nbytes;

    assure(wp);
    assure(wp->putfd >= 0);
    assure(wp->input.buf);

    nbytes = bufLen(&wp->input);
    wp->putLen += nbytes;
    if (wp->putLen > BIT_LIMIT_PUT) {
        websError(wp, HTTP_CODE_REQUEST_TOO_LARGE | WEBS_CLOSE, "Put file too large");
        return -1;
    }
    if (write(wp->putfd, wp->input.servp, (int) nbytes) != nbytes) {
        websError(wp, HTTP_CODE_INTERNAL_SERVER_ERROR | WEBS_CLOSE, "Can't write to file");
        return -1;
    }
    websConsumeInput(wp, nbytes);
    return 0;
}


static void fileClose()
{
    gfree(websIndex);
    websIndex = NULL;
    gfree(websDocuments);
    websDocuments = NULL;
}


PUBLIC void websFileOpen()
{
    websIndex = strdup("index.html");
    websDefineHandler("file", fileHandler, fileClose, 0);
}


/*
    Get the default page for URL requests ending in "/"
 */
PUBLIC char *websGetIndex()
{
    return websIndex;
}


PUBLIC char *websGetDocuments()
{
    return websDocuments;
}


/*
    Set the default page for URL requests ending in "/"
 */
PUBLIC void websSetIndex(char *page)
{
    assure(page && *page);

    if (websIndex) {
        gfree(websIndex);
    }
    websIndex = strdup(page);
}


/*
    Set the default web directory
 */
PUBLIC void websSetDocuments(char *dir)
{
    assure(dir && *dir);
    if (websDocuments) {
        gfree(websDocuments);
    }
    websDocuments = strdup(dir);
}

/*
    @copy   default

    Copyright (c) Embedthis Software LLC, 2003-2012. All Rights Reserved.

    This software is distributed under commercial and open source licenses.
    You may use the Embedthis GoAhead open source license or you may acquire 
    a commercial license from Embedthis Software. You agree to be fully bound
    by the terms of either license. Consult the LICENSE.md distributed with
    this software for full details and other copyrights.

    Local variables:
    tab-width: 4
    c-basic-offset: 4
    End:
    vim: sw=4 ts=4 expandtab

    @end
 */
