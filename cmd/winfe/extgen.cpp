/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/*  Author:
 *      Garrett Arch Blythe
 *      blythe@netscape.com
 */

#include "stdafx.h"
#include "extgen.h"

/*-----------------------------------------------------------------------**
** Things you do not care about, private internal implemenation details. **
**-----------------------------------------------------------------------*/
size_t ext_Extract(char *pOutExtension, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName);
BOOL ext_ExtByIndex(int iIndex, char *pExt, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName, const char *pMimeType);
BOOL ext_PreserveExt(const char *pExt, DWORD dwFlags, const char *pOrigName);
BOOL ext_ShellExecutable(const char *pExt, DWORD dwFlags);
BOOL ext_PreserveMime(const char *pExt, const char *pMimeType);
BOOL ext_ExtByMimeIndex(int iIndex, char *pExt, size_t stExtBufSize, DWORD dwFlags, const char *pMimeType);
#ifdef XP_WIN32
void ext_MimeDatabase(char *pExt, size_t stExtBufSize, DWORD dwFlags, const char *pMimeType);
#endif

size_t EXT_Invent(char *pOutExtension, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName, const char *pMimeType)
{
    //  Ensure we have a buffer to provide consistent functionality if
    //      the buffer is missing.
    char aBuffer[_MAX_EXT];
    if(!pOutExtension) {
        pOutExtension = aBuffer;
        stExtBufSize = sizeof(aBuffer);
    }
    
    //  You may want to provide a big enough buffer to support extensions
    //      on the native file system.
    ASSERT(stExtBufSize >= _MAX_EXT);
    
    //  Limit in parameters.
    //  We only allow the no period flag, and dot three flag,
    //      as we manipulate the other flags herein.
    ASSERT((dwFlags & (EXT_NO_PERIOD | EXT_DOT_THREE)) == dwFlags);
    dwFlags &= (EXT_NO_PERIOD | EXT_DOT_THREE);
    
    //  Initialize out parameters.
    size_t stReturns = 0;
    *pOutExtension = '\0';
    
    //  We want to preserve the file extension, respect the MIME type,
    //      and want the Shell to know how to handle the file.
    //  This is absolutely the best scenario.
    if(!stReturns) {
        DWORD dwPassFlags = dwFlags |
            EXT_PRESERVE_EXT |
            EXT_PRESERVE_MIME |
            EXT_SHELL_EXECUTABLE |
            EXT_EXECUTABLE_IDENTITY;
            
        stReturns = EXT_Generate(pOutExtension, stExtBufSize, dwPassFlags, pOrigName, pMimeType);
    }
    
    //  We want to respect MIME type over file extension, but we also
    //      want the Shell to understand how to use the file.  Try to
    //      find any extension under that MIME type which the Shell
    //      will handle and use it.
    if(!stReturns) {
        DWORD dwPassFlags = dwFlags |
            EXT_PRESERVE_MIME |
            EXT_SHELL_EXECUTABLE |
            EXT_EXECUTABLE_IDENTITY;
            
        stReturns = EXT_Generate(pOutExtension, stExtBufSize, dwPassFlags, pOrigName, pMimeType);
    }
    
    //  We give up on respecting MIME type, and we just want the Shell
    //      to be able to handle the file.  Is the current file
    //      extension good enough?
    if(!stReturns) {
        DWORD dwPassFlags = dwFlags |
            EXT_PRESERVE_EXT |
            EXT_SHELL_EXECUTABLE |
            EXT_EXECUTABLE_IDENTITY;
            
        stReturns = EXT_Generate(pOutExtension, stExtBufSize, dwPassFlags, pOrigName, pMimeType);
    }
    
    //  Extension has no shell association.  Would be nice if the extension
    //      were in the MIME list, however.
    if(!stReturns) {
        DWORD dwPassFlags = dwFlags |
            EXT_PRESERVE_EXT |
            EXT_PRESERVE_MIME;
            
        stReturns = EXT_Generate(pOutExtension, stExtBufSize, dwPassFlags, pOrigName, pMimeType);
    }
    
    //  Extension has no shell association.  We want to respect
    //      MIME type over filename, as we might want to know the MIME
    //      type some time in the future.  See if we have an extension
    //      for the MIME type.
    if(!stReturns) {
        DWORD dwPassFlags = dwFlags |
            EXT_PRESERVE_MIME;
            
        stReturns = EXT_Generate(pOutExtension, stExtBufSize, dwPassFlags, pOrigName, pMimeType);
    }
    
    //  Extension has no shell association.  We have no extension for the MIME type.
    //  Simply give up and use the extension of the file if present.
    if(!stReturns) {
        DWORD dwPassFlags = dwFlags |
            EXT_PRESERVE_EXT;
            
        stReturns = EXT_Generate(pOutExtension, stExtBufSize, dwPassFlags, pOrigName, pMimeType);
    }
    
    //  All done.
    return(stReturns);
}    

size_t EXT_Generate(char *pOutExtension, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName, const char *pMimeType)
{
    //  Ensure we have a buffer to provide consistent functionality if
    //      the buffer is missing.
    char aBuffer[_MAX_EXT];
    if(!pOutExtension) {
        pOutExtension = aBuffer;
        stExtBufSize = sizeof(aBuffer);
    }
    
    //  You may want to provide a big enough buffer to support extensions
    //      on the native file system.
    ASSERT(stExtBufSize >= _MAX_EXT);
    
    //  Initialize out parameters.
    *pOutExtension = '\0';
    
    //  Get the period business out of the way first.
    char *pExt = pOutExtension;
    if(!(dwFlags & EXT_NO_PERIOD) && (stExtBufSize > 1)) {
        //  We use strcat so that the string is NULL terminated.
        strcat(pExt, ".");
        pExt++;
    }
    
    //  We do not want to change the extension for a mime type of
    //      application/octet-stream or applciation/x-msdownload.
    //  In some cases, these files are binary and we used to miss
    //      name them with .EXE extensions.
    if(pMimeType) {
        BOOL bPreserveExtension = FALSE;
        if(0 == stricmp(APPLICATION_OCTET_STREAM, pMimeType)) {
            bPreserveExtension = TRUE;
        }
        else if(0 == stricmp("application/x-msdownload", pMimeType)) {
            bPreserveExtension = TRUE;
        }
        
        if(bPreserveExtension) {
            dwFlags |= EXT_PRESERVE_EXT;
        }
    }
    
    //  This loop traverses all extensions possible, and will verify
    //      that the extension matches all flags before continuing.
    BOOL bSuccess = FALSE;
    for(int iIndex = 0; ext_ExtByIndex(iIndex, pExt, stExtBufSize - (pExt - pOutExtension), dwFlags, pOrigName, pMimeType); iIndex++) {
        //  Do not check empty extensions.
        if(!*pExt) {
            continue;
        }
        
        //  See if the extension holds up to the flags.
        if((dwFlags & EXT_PRESERVE_EXT) && !ext_PreserveExt(pExt, dwFlags, pOrigName)) {
            continue;
        }
        if((dwFlags & EXT_SHELL_EXECUTABLE) && !ext_ShellExecutable(pExt, dwFlags)) {
            continue;
        }
        if((dwFlags & EXT_PRESERVE_MIME) && !ext_PreserveMime(pExt, pMimeType)) {
            continue;
        }
        
        //  Made it through all checks.
        //  Extension is good.
        bSuccess = TRUE;
        break;
    }
    
    //  Finally, if were going to fail, lets give caller
    //      an empty buffer to work with (to help get around
    //      those people that dont check error codes).
    if(!bSuccess) {
        *pOutExtension = '\0';
    }
    
    //  All done.
    return(strlen(pOutExtension));
}

/*--------------------------------------------------------------------**
** Goal is to provide an abstract way to iterate through all possible **
**     extensions these attributes can produce.                       **
** Caller must know to ignore empty strings if we return TRUE to      **
**      avoid having intimate knowledge about what we do inside here. **
**                                                                    **
** WIN32:  Offset will be greater since were doing one special        **
**     step.                                                          **
** EXT_OFFSET if subtracted from iIndex will yield a zero index into  **
**     another list.                                                  **
**--------------------------------------------------------------------*/
#ifdef XP_WIN32
#define EXT_OFFSET 2
#else
#define EXT_OFFSET 1
#endif
BOOL ext_ExtByIndex(int iIndex, char *pExt, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName, const char *pMimeType)
{
    //  We assume a few things in the internal implementation.
    ASSERT(pExt);

    //  Set out parameters.
    BOOL bReturns = FALSE;
    *pExt = '\0';
    
    if(!iIndex) {
        //  First extension should always be that of the file.
        //  Always return TRUE, such that the following steps are
        //      executed.
        ext_Extract(pExt, stExtBufSize, dwFlags, pOrigName);
        bReturns = TRUE;
    }
#ifdef XP_WIN32
    else if (1 == iIndex) {
        //  Check the MIME database.
        //  Always return TRUE, such that the following steps are
        //      executed.
        ext_MimeDatabase(pExt, stExtBufSize, dwFlags, pMimeType);
        bReturns = TRUE;
    }
#endif
    else {
        //  Iterate through extensions found by MIME type.
        //  Notice that the index is correct on Win32 and
        //      Win16 due to compile time manipulation
        //      of the offset.
        int iMimeIndex = iIndex - EXT_OFFSET;
        bReturns = ext_ExtByMimeIndex(iMimeIndex, pExt, stExtBufSize, dwFlags, pMimeType);
    }
    
    //  Return wether or not another extension is returned.
    return(bReturns);
}
#undef EXT_OFFSET

/*-----------------------------------------------------------------**
** Goal is to specifically iterate our interally kept mime list(s) **
** Caller should know how to ignore empty strings with successful  **
**      return value.
**-----------------------------------------------------------------*/
BOOL ext_ExtByMimeIndex(int iIndex, char *pExt, size_t stExtBufSize, DWORD dwFlags, const char *pMimeType)
{
    //  We assume a few things in the internal implementation.
    ASSERT(pExt);
    
    //  Clear any out params.
    BOOL bReturns = FALSE;
    *pExt = '\0';
    
    //  No need to attempt if no Mime type.
    if(pMimeType) {
        //  Find the list of extensions of that MIME type.
        XP_List *pTypesList = cinfo_MasterListPointer();
        NET_cdataStruct *pListEntry = NULL;
        while ((pListEntry = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
            if(pListEntry->ci.type != NULL)	{
                if(!stricmp(pListEntry->ci.type, pMimeType)) {
                    if(pListEntry->num_exts > iIndex) {
                        //  We consider getting this far success.
                        bReturns = TRUE;
                        
                        char *pFinally = pListEntry->exts[iIndex];
                        if(pFinally) {
                            if('.' == *pFinally) {
                                //  Go beyond the period.
                                pFinally++;
                            }
                            
                            //  See if it matches the flags.
                            BOOL bGood = FALSE;
                            if(strlen(pFinally) < stExtBufSize) {
                                if(dwFlags & EXT_DOT_THREE) {
                                    if(strlen(pFinally) <= 3) {
                                        bGood = TRUE;
                                    }
                                }
                                else {
                                    bGood = TRUE;
                                }
                            }
                            
                            if(bGood) {
                                strcpy(pExt, pFinally);
                            }
                        }
                    }
                    //  No need to continue loop after MIME type found.
                    break;
                }
            }
        }
    }
    
    return(bReturns);
}

/*------------------------------------------------------------------**
** Goal is to determine wether or not the extension of the original **
**     file is preserved in the extension passed in.                **
**------------------------------------------------------------------*/
BOOL ext_PreserveExt(const char *pExt, DWORD dwFlags, const char *pOrigName)
{
    BOOL bRetval = FALSE;

    if(pExt) {
        char aBuffer[_MAX_EXT];
        aBuffer[0] = '\0';
        
        if(pOrigName) {
            //  Extract the extension.
            //  Dont check return value.
            //  Well want to return true if both strings are emptpy.
            size_t stOrig = ext_Extract(aBuffer, sizeof(aBuffer), dwFlags, pOrigName);
            if(!stOrig) {
                aBuffer[0] = '\0';
            }
        }
        
        if(!stricmp(aBuffer, pExt)) {
            //  Good.
            bRetval = TRUE;
        }
    }
    
    return(bRetval);
}

/*-----------------------------------------------------------------**
** Goal is to determine wether or not the given extension is shell **
**     executable.                                                 **
**-----------------------------------------------------------------*/
BOOL ext_ShellExecutable(const char *pExt, DWORD dwFlags)
{
    BOOL bRetval = FALSE;
    
    //  Empty strings fail.
    if(pExt) {
        //  Pass off to executable finder, which should just
        //      rely on the shell to find the information with a
        //      little extra logic.
        //  We'll need to add a period.
        char *pBuffer = (char *)XP_ALLOC(strlen(pExt) + 2);
        if(pBuffer) {
            strcpy(pBuffer, ".");
            strcat(pBuffer, pExt);
            bRetval = FEU_FindExecutable(pBuffer, NULL, dwFlags & EXT_EXECUTABLE_IDENTITY ? TRUE : FALSE, TRUE);
            XP_FREE(pBuffer);
            pBuffer = NULL;
        }
    }
    
    return(bRetval);
}

/*----------------------------------------------------------------------**
** Goal is to determine wether or not the passed in extension correctly **
**     represents the MIME type.                                        **
**----------------------------------------------------------------------*/
BOOL ext_PreserveMime(const char *pExt, const char *pMimeType)
{
    BOOL bRetval = FALSE;
    
    //  Need an extension and a MIME type to continue.
    //  We need both so that we can make sure the extension is in
    //      the mime type list.
    if(pExt && pMimeType) {
        //  Find the MIME type in our internal list.
        //  We will not depend on any external information,
        //      as all the inner workings are actually
        //      controlled by our inner list, regardless of
        //      how the system actually has things mapped out.
        //  Find the list of extensions of that MIME type.
        XP_List *pTypesList = cinfo_MasterListPointer();
        NET_cdataStruct *pListEntry = NULL;
        while ((pListEntry = (NET_cdataStruct *)XP_ListNextObject(pTypesList)))	{
            if(pListEntry->ci.type != NULL)	{
                if(!stricmp(pListEntry->ci.type, pMimeType)) {
                    //  See if extension is in the list.
                    char *pListExt;
                    for(int iNum = 0; iNum < pListEntry->num_exts; iNum++) {
                        pListExt = pListEntry->exts[iNum];
                        if('.' == *pListExt) {
                            //  No need to compare with period.
                            pListExt++;
                        }
                        
                        //  Compare without case.
                        if(!stricmp(pListExt, pExt)) {
                            //  Mime type is represented by the extension.
                            //  Preserved.
                            bRetval = TRUE;
                            break;
                        }
                    }
                    //  No need to continue loop after MIME type found.
                    break;
                }
            }
        }
    }
    
    return(bRetval);
}

/*-------------------------------------------------------------------------**
** Goal is to simply strip off the end of the filename without overflowing **
**     the buffer.  We do not care about adding a period.                  **
**-------------------------------------------------------------------------*/
size_t ext_Extract(char *pOutExtension, size_t stExtBufSize, DWORD dwFlags, const char *pOrigName)
{
    size_t stReturns = 0;

    if(pOutExtension) {
        if(pOrigName) {
            //  Make a copy of the name we can mess with.
            char *pMuck = XP_STRDUP(pOrigName);
            if(pMuck) {
                //  Want to discard anyting beyond a '?' (get form post)
                //      or a '#' (named anchor).
                char *pDiscard;
                if(pDiscard = strchr(pMuck, '?')) {
                    *pDiscard = '\0';
                }
                if(pDiscard = strchr(pMuck, '#')) {
                    *pDiscard = '\0';
                }
                
                //  Find the trailing slash or backslash.
                char *pForeSlash = strrchr(pMuck, '/');
                char *pBackSlash = strrchr(pMuck, '\\');
                char *pSlash = pForeSlash > pBackSlash ? pForeSlash : pBackSlash;
                
                //  Find the trailing period.
                char *pPeriod = strrchr(pMuck, '.');
                
                //  Period must be present.
                //  Period must come after slash.
                if(pPeriod && (pPeriod > pSlash)) {
                    //  Go one past the period.
                    pPeriod++;
                    
                    //  Go over until end of buffer is reached,
                    //      until we hit end of string, or
                    //      until we hit a non-alpha-numeric character.
                    //  We could support spaces and other valid fname
                    //      chars, but we havent up to this point
                    //      and see no need to further complicate the
                    //      issues right now.
                    while(*pPeriod && isalnum(*pPeriod) && (stExtBufSize - 1)) {
                        *pOutExtension = *pPeriod;
                        pPeriod++;
                        pOutExtension++;
                        stExtBufSize--;
                        stReturns++;
                        
                        //  Consider 8.3
                        if((dwFlags & EXT_DOT_THREE) && (stReturns >= 3)) {
                            break;
                        }
                    }
                }
                XP_FREE(pMuck);
                pMuck = NULL;
            }
        }
        //  End the out parameter (or sets to empty on failure).
        ASSERT(stExtBufSize);
        *pOutExtension = '\0';
    }
    
    return(stReturns);
}

#ifdef XP_WIN32
/*------------------------------------------------------------------------**
** Goal is to look up the mime type and extension in the system registry. **
**------------------------------------------------------------------------*/
void ext_MimeDatabase(char *pExt, size_t stExtBufSize, DWORD dwFlags, const char *pMimeType)
{
    //  Clear out params.
    ASSERT(pExt);
    *pExt = '\0';

    if(pMimeType) {
        //  Open up the MIME database.
        HKEY hMime = NULL;
        char aMime[_MAX_PATH];
        strcpy(aMime, "MIME\\Database\\Content Type\\");
        strcat(aMime, pMimeType);

        LONG lCheckOpen = RegOpenKeyEx(HKEY_CLASSES_ROOT, aMime, 0, KEY_QUERY_VALUE, &hMime);
        if(ERROR_SUCCESS == lCheckOpen) {
            //  Determine default extension.
            char aExt[_MAX_EXT];
            aExt[0] = '\0';
            DWORD dwExtSize = sizeof(aExt);
            LONG lCheckQuery = RegQueryValueEx(hMime, "Extension", NULL, NULL, (unsigned char *)aExt, &dwExtSize);
            LONG lCheckClose = RegCloseKey(hMime);
            ASSERT(ERROR_SUCCESS == lCheckClose);
            hMime = NULL;
            
            if(ERROR_SUCCESS == lCheckQuery) {
                char *pEval = aExt;
                if('.' == *pEval) {
                    //  Go beyond period, dont want it.
                    pEval++;
                }
                
                if(*pEval) {
                    BOOL bGood = FALSE;
                    
                    //  Got an extension.
                    //  See if it is suitable.
                    //  First, can it fit in our buffer?
                    if(strlen(pEval) < stExtBufSize) {
                        //  Check any relevant flags.
                        if(dwFlags & EXT_DOT_THREE) {
                            if(strlen(pEval) <= 3) {
                                bGood = TRUE;
                            }
                        }
                        else {
                            bGood = TRUE;
                        }
                    }
                    
                    if(bGood) {
                        //  Good match.
                        strcpy(pExt, pEval);
                    }
                }
            }
        }
    }
}
#endif


