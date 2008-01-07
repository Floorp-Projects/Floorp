/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code, released
 * Jan 28, 2003.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Garrett Arch Blythe, 28-January-2003
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "mozce_internal.h"

extern "C" {
#if 0
}
#endif

/*
**  One day, these multi-byte routines will need to really do thier job.
**
**  Right now, bail with a default implementation.
*/

#define LOG_CALLS

MOZCE_SHUNT_API unsigned char* _mbsinc(const unsigned char* inCurrent)
{
    MOZCE_PRECHECK

#ifdef LOG_CALLS
#ifdef DEBUG
    mozce_printf("mbsinc called\n");
#endif
#endif
    //IsDBCSLeadByte(path[len-1])
    return (unsigned char*)(inCurrent + 1);
}


MOZCE_SHUNT_API unsigned char* _mbspbrk(const unsigned char* inString, const unsigned char* inStrCharSet)
{
    MOZCE_PRECHECK

#ifdef LOG_CALLS
#ifdef DEBUG
    mozce_printf("mbspbrk called\n");
#endif
#endif

    LPWSTR wstring = a2w_malloc((const char *)inString, -1, NULL);
    LPWSTR wset    = a2w_malloc((const char *)inStrCharSet, -1, NULL);
    LPWSTR result  = wcspbrk(wstring, wset);
    free(wstring);
    free(wset);
    return (unsigned char *)result;
}


MOZCE_SHUNT_API unsigned char* mbsrchr(const unsigned char* inString, unsigned int inC)
{
    MOZCE_PRECHECK

#ifdef LOG_CALLS
#ifdef DEBUG
    mozce_printf("mbsrchr called\n");
#endif
#endif

    return (unsigned char*) strrchr((char*)inString, inC);
}


MOZCE_SHUNT_API unsigned char* mbschr(const unsigned char* inString, unsigned int inC)
{
    MOZCE_PRECHECK

#ifdef LOG_CALLS
#ifdef DEBUG
    mozce_printf("mbschr called\n");
#endif
#endif
    return (unsigned char*)strchr((const char*)inString, (int)inC);
}


MOZCE_SHUNT_API int mbsicmp(const unsigned char *string1, const unsigned char *string2)
{
    MOZCE_PRECHECK

#ifdef LOG_CALLS
#ifdef DEBUG
    mozce_printf("mbsicmp called\n");
#endif
#endif
    return _stricmp((const char*)string1, (const char*)string2);
}

MOZCE_SHUNT_API unsigned char* mbsdec(const unsigned char *string1, const unsigned char *string2)
{
    MOZCE_PRECHECK

#ifdef LOG_CALLS
#ifdef DEBUG
    mozce_printf("mbsdec called\n");
#endif
#endif
    
    if (string1 == string2)
        return 0;
    
    return (unsigned char *)string2 - 1;
}

#if 0
{
#endif
} /* extern "C" */

