/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/*
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, 
 * released March 31, 1998. 
 *
 * The Initial Developer of the Original Code is Netscape Communications 
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *     Samir Gehani <sgehani@netscape.com>
 */


#include "nsINIParser.h"
#include "nsCRT.h"

nsINIParser::nsINIParser(const char *aFilename)
{
    FILE    *fd = NULL;
    long    eofpos = 0;
    int     rd = 0;

    mFileBuf = NULL;
    mFileBufSize = 0;
    mError = OK;
    DUMP("nsINIParser");

    /* param check */
    if (!aFilename)
    {
        mError = E_PARAM;
        return;
    }

    /* open the file */
    fd = fopen(aFilename, "r");
    if (!fd)
        goto bail;
    
    /* get file size */
    if (fseek(fd, 0, SEEK_END) != 0)
        goto bail;
    eofpos = ftell(fd);
    if (eofpos == 0)
        goto bail;

    /* malloc an internal buf the size of the file */
    mFileBuf = (char *) malloc((eofpos+1) * sizeof(char));
    if (!mFileBuf)
    {
        mError = E_MEM;
        return;
    }
    mFileBufSize = eofpos;

    /* read the file in one swoop */
    if (fseek(fd, 0, SEEK_SET) != 0)
        goto bail;
    rd = fread((void *)mFileBuf, 1, eofpos, fd);
    if (!rd)
        goto bail;
    mFileBuf[mFileBufSize] = '\0';

    /* close file */
    fclose(fd);

    return;

bail:
    mError = E_READ;
    return;
}

nsINIParser::~nsINIParser()
{
    if (mFileBuf) {
        nsCRT::free(mFileBuf);
        mFileBuf = nsnull;
    }
    DUMP("~nsINIParser");
}

int
nsINIParser::GetString( char *aSection, char *aKey, 
                        char *aValBuf, int *aIOValBufSize )
{
    char *secPtr = NULL;
    mError = OK;
    DUMP("GetString");

    /* param check */
    if ( !aSection || !aKey || !aValBuf || 
         !aIOValBufSize || (*aIOValBufSize <= 0) )
        return E_PARAM;

    /* find the section if it exists */
    mError = FindSection(aSection, &secPtr);
    if (mError != OK)
        return mError;

    /* find the key if it exists in the valid section we found */
    mError = FindKey(secPtr, aKey, aValBuf, aIOValBufSize);

    return mError;
}

int
nsINIParser::GetStringAlloc( char *aSection, char *aKey,
                             char **aOutBuf, int *aOutBufSize )
{
    char buf[MAX_VAL_SIZE];
    int bufsize = MAX_VAL_SIZE;
    mError = OK;
    DUMP("GetStringAlloc");

    mError = GetString(aSection, aKey, buf, &bufsize);
    if (mError != OK)
        return mError;

    *aOutBuf = (char *) malloc(bufsize + 1);
    strncpy(*aOutBuf, buf, bufsize);
    *(*aOutBuf + bufsize) = 0;
    *aOutBufSize = bufsize + 1;

    return mError;
}

int
nsINIParser::GetError()
{
    DUMP("GetError");
    return mError;
}

char *
nsINIParser::ResolveName(char *aINIRoot)
{
    char *resolved = NULL;
    char *locale = NULL;
    struct stat st_exists;

    /* param check */
    if (!aINIRoot)
        return NULL;

    locale = setlocale(LC_CTYPE, NULL);
    if (!locale) 
        return NULL;

    /* resolved string: "<root>.ini.<locale>\0" */
    resolved = (char *) malloc(strlen(aINIRoot) + 5 + strlen(locale) + 1);
    if (!resolved)
        return NULL;

    /* locale specific ini file name */
    sprintf(resolved, "%s.ini.%s", aINIRoot, locale);
    if (0 == stat(resolved, &st_exists))
        return resolved;

    /* fallback to general ini file name */
    sprintf(resolved, "%s.ini", aINIRoot);
    if (0 == stat(resolved, &st_exists))
        return resolved;
    
    /* neither existed so error returned */
    return NULL;
}

int
nsINIParser::FindSection(char *aSection, char **aOutSecPtr)
{
    char *currChar = mFileBuf;
    char *nextSec = NULL;
    char *secClose = NULL;
    char *nextNL = NULL;
    mError = E_NO_SEC;
    DUMP("FindSection");

    // param check
    if (!aSection || !aOutSecPtr)
    {
        mError = E_PARAM;
        return mError;
    }

    while (currChar < (mFileBuf + mFileBufSize))
    {
        // look for first '['
        nextSec = NULL;
        nextSec = strchr(currChar, '[');
        if (!nextSec)
            break;
            
        currChar = nextSec + 1;

        // extract section name till first ']'
        secClose = NULL; nextNL = NULL;
        secClose = strchr(currChar, ']');
        nextNL = strchr(currChar, NL);
        if ((!nextNL) || (nextNL < secClose))
        {
            currChar = nextNL;
            continue;
        }

        // if section name matches we succeeded
        if (strncmp(aSection, currChar, strlen(aSection)) == 0)
        {
            *aOutSecPtr = secClose + 1;
            mError = OK;
            break;
        }
    }

    return mError;
}

int
nsINIParser::FindKey(char *aSecPtr, char *aKey, char *aVal, int *aIOValSize)
{
    char *nextNL = NULL;
    char *secEnd = NULL;
    char *currLine = aSecPtr;
    char *nextEq = NULL;
    mError = E_NO_KEY;
    DUMP("FindKey");

    // param check
    if (!aSecPtr || !aKey || !aVal || !aIOValSize || (*aIOValSize <= 0))
    {
        mError = E_PARAM;
        return mError;
    }

    // determine the section end
    secEnd = aSecPtr;
find_end:
    if (secEnd)
        secEnd = strchr(secEnd, '['); // search for next sec start
    if (!secEnd)
    {
        secEnd = strchr(aSecPtr, '\0'); // else search for file end
        if (!secEnd)
        {
            mError = E_SEC_CORRUPT; // else this data is corrupt
            return mError;
        }
    }

    // handle start section token ('[') in values for i18n
    if (*secEnd == '[' && !(secEnd == aSecPtr || *(secEnd-1) == NL))
    {
        secEnd++;
        goto find_end;
    }

    while (currLine < secEnd)
    {
        nextNL = NULL;
        nextNL = strchr(currLine, NL);
        if (!nextNL)
            nextNL = mFileBuf + mFileBufSize;

        // ignore commented lines (starting with ;)
        if (currLine == strchr(currLine, ';'))
        {
            currLine = nextNL + 1;
            continue;
        }

        // extract key before '='
        nextEq = NULL;
        nextEq = strchr(currLine, '=');
        if (!nextEq || nextEq > nextNL) 
        {
            currLine = nextNL + 1;
            continue;
        }

        // if key matches we succeeded
        if (strncmp(currLine, aKey, strlen(aKey)) == 0)
        {
            // extract the value and return
            if (*aIOValSize < nextNL - nextEq)
            {
                mError = E_SMALL_BUF;
                *aVal = '\0';
                *aIOValSize = 0;
                return mError;
            }
                
            *aIOValSize = nextNL - (nextEq + 1); 
            strncpy(aVal, (nextEq + 1), *aIOValSize);
            *(aVal + *aIOValSize) = 0; // null terminate
            mError = OK;
            return mError;
        }
        else
        {
            currLine = nextNL + 1;
        }
    }

    return mError;
}
