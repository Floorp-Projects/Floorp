/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "xp.h"
#include "prmem.h"
#include "plstr.h"

// For xp to ns file translation
#include "nsVoidArray.h"
#ifndef XP_UNIX
	#ifndef XP_MAC
	#include "direct.h"
	#endif
#endif

#include "nsIComponentManager.h" 
#include "oldnsINetFile.h"

// The nsINetfile
static nsINetFile *fileMgr = nsnull;


typedef struct _xp_to_nsFile {
    nsFile *nsFp;
    XP_File xpFp;
} xpNSFile;

// Array of translation structs from xp to ns file.
static nsVoidArray switchBack;

// Utility routine to remove a transator object from the array and free it.
nsresult deleteTrans(nsFile *nsFp) {
    xpNSFile *trans = nsnull;

    if (!nsFp)
        return NS_OK;

    for (PRInt32 i = switchBack.Count(); i > 0; i--) {
        trans = (xpNSFile*)switchBack.ElementAt(i-1);
        if (trans && trans->nsFp == nsFp) {
            switchBack.RemoveElement(trans);
            if (trans->xpFp) {
                PR_Free(trans->xpFp);
                trans->xpFp = nsnull;
            }
            PR_Free(trans);
            return NS_OK;
        }
    }
    return NS_ERROR_FAILURE;

}

PRIVATE
char *xpFileTypeToName(XP_FileType type) {
    char *name = nsnull;
    switch (type) {
        case (xpCache):
            return PL_strdup(CACHE_DIR_TOK);
            break;
        case (xpCacheFAT):
            return PL_strdup(CACHE_DIR_TOK CACHE_DB_F_TOK);
            break;
        case (xpProxyConfig):
            break;
        case (xpURL):
            break;
        case (xpJSConfig):
            break;
        case (xpTemporary):
            break;
        case (xpJSCookieFilters):
            break;
        case (xpFileToPost):
            break;
        case (xpMimeTypes):
            break;
        case (xpHTTPCookie):
            return PL_strdup("%USER%%COOKIE_F%");
#if defined(CookieManagement)
        case (xpHTTPCookiePermission):
            return PL_strdup("%USER%%COOKIE_PERMISSION_F%");
#endif
     //   case (xpHTTPSingleSignon):
     //       return PL_strdup("%USER%%SIGNON_F%");

        default:
            break;
    }
    return nsnull;
}

// Utility routine to convert an xpFile pointer to a ns file pointer.
nsFile * XpToNsFp(XP_File xpFp) {
    nsFile *nsFp = nsnull;
    xpNSFile *trans = nsnull;

    if (!xpFp)
        return nsnull;
    for (PRInt32 i = switchBack.Count(); i > 0; i--) {
        trans = (xpNSFile*)switchBack.ElementAt(i-1);
        if (trans && trans->xpFp == xpFp) {
            nsFp = trans->nsFp;
            break;
        }
    }
    return nsFp;
}

extern "C"  XP_File 
NET_I_XP_FileOpen(const char * name, XP_FileType type, const XP_FilePerm perm)
{
    XP_File xpFp;
    xpNSFile *trans = (xpNSFile*)PR_Malloc(sizeof(xpNSFile));
    nsFile *nsFp = nsnull;
    nsresult rv;
    nsFileMode mode;
    char *aName = nsnull;

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return NULL;
        }
    }

    // Just get some random address.
    xpFp = (XP_File) PR_Malloc(1);

    trans->xpFp= xpFp;

    if (!PL_strcasecmp(perm,XP_FILE_READ)) {
        mode = nsRead;
    } else if (!PL_strcasecmp(perm,XP_FILE_WRITE)) {
        mode = nsOverWrite;
    } else if (!PL_strcasecmp(perm,XP_FILE_WRITE_BIN)) {
        mode = nsOverWrite;
    } else {
        mode = nsReadWrite;
    }

    /* call OpenFile with nsNetFile syntax if necesary. */
    if ( (!name || !*name) 
         || type == xpCache ) {
        char *tmpName = xpFileTypeToName(type);
        nsString newName = tmpName;
        PR_FREEIF(tmpName)
        if (newName.Length() < 1) {
            PR_Free(trans);
            PR_Free(xpFp);
            return NULL;
        }
        newName.Append(name);
        aName = newName.ToNewCString();
    }

    rv = fileMgr->OpenFile( (aName ? aName : name), mode, &nsFp);
    if (aName)
        delete [] aName;
    if (NS_OK != rv) {
        return NULL;
    }

    trans->nsFp = nsFp;

    switchBack.AppendElement(trans);

    return xpFp;
}

extern "C"  char *
NET_I_XP_FileReadLine(char *outBuf, int outBufLen, XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    PRInt32 readBytes;
    nsresult rv;

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer");

    if (!nsFp)
        return NULL;

    if (!fileMgr)
        return NULL;

    rv = fileMgr->FileReadLine( nsFp, &outBuf, &outBufLen, &readBytes);
    if (NS_OK != rv) {
        return NULL;
    }

    return outBuf;
}

extern "C"  int
NET_I_XP_FileWrite(const char *buf, int bufLen, XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    PRInt32 wroteBytes;
    PRInt32 len;
    nsresult rv;

    if (bufLen < 0)
        len = PL_strlen(buf);
    else
        len = bufLen;

    if (!nsFp)
        return -1;

    if (!fileMgr)
        return NULL;

    rv = fileMgr->FileWrite(nsFp, buf, &len, &wroteBytes);;
    if (rv != NS_OK)
        return NULL;

    return (int) wroteBytes;
}

extern "C" int
NET_I_XP_FileClose(XP_File fp) {
    nsFile *nsFp = XpToNsFp(fp); // nsnull ok.
    nsresult rv;

    NS_PRECONDITION( (nsFp != nsnull), "Null pointer");

    if (!fileMgr) {
        if (NS_NewINetFile(&fileMgr, nsnull) != NS_OK) {
            return 0;
        }
    }

    rv = fileMgr->CloseFile(nsFp);
    if (rv != NS_OK)
        return 0;

    deleteTrans(nsFp);
    fp = nsnull;

    return 1;
}

