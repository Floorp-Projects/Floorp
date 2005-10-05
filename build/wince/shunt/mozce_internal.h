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



#if !defined __mozce_internal_h
#define __mozce_internal_h

#include <windows.h>

#include "mozce_defs.h"

/*
**  Perform the requested conversion using the buffer provided.
**
**  inACPString     The wide character string to convert.
**  inACPChars      Count of acp multibyte characters in inACPString to be
**                      converted.
**                  If -1, assume a terminated string and the terminating
**                      character will also be appended to outWideString.
**  outWideString   The buffer to store the converted string.
**  inWideChars     Number of characters (not bytes) outWideString can hold.
**                  If this value is zero, then the character count required
**                      for the conversion is returned and outWideString is
**                      untouched.
**  returns int     The number of characters (not bytes) converted/required.
**                  Zero indicates failure.
**                  Generally you could use this value - 1 to avoid a
**                      wcslen() call after the conversion took place
**                      should the string be terminated (i.e. if inACPChars
**                      included a terminating character for the conversion).
*/

int a2w_buffer(const char* inACPString, int inACPChars, unsigned short* outWideString, int inWideChars);

/*
**  Perform the requested conversion using heap memory.
**  The caller/client of this function must use free() to release the
**      resultant string to the heap once finished with said string.
**  This function is best used when the conversion length of inACPString
**      is not known beforehand.
**
**  inACPString     The acp multibyte character string to convert.
**  inACPChars      Count of acp multibyte characters in inACPString to be
**                      converted.
**                  If -1, assume a terminated string and the terminating
**                      character will also be appended to the return value.
**  outWideChars    Optional argument, can be NULL.
**                  Holds number of characters (not bytes) written into
**                      return value.
**                  Generally you would use outWideChars - 1 to avoid a
**                      wcslen() call after the conversion took place
**                      should the string be terminated (i.e. if inACPChars
**                      included a terminating character for the conversion).
**  returns LPWSTR  The malloced converted string which must eventually be
**                      free()d.
**                  NULL on failure.
*/

unsigned short* a2w_malloc(const char* inACPString, int inACPChars, int* outWideChars);

/*
**  Perform the requested conversion using the buffer provided.
**
**  inWideString    The wide character string to convert.
**  inWideChars     Count of wide characters (not bytes) in
**                      inWideString to be converted.
**                  If -1, assume a terminated string and the terminating
**                      character will also be appended to outACPString.
**  outACPString    The buffer to store the converted string.
**  inACPChars      Number of characters outACPString can hold.
**                  If this value is zero, then the character count required
**                      for the conversion is returned and outACPString is
**                      untouched.
**  returns int     The number of characters converted or required.
**                  Zero indicates failure.
**                  Generally you could use this value - 1 to avoid a
**                      strlen() call after the conversion took place
**                      should the string be terminated (i.e. if inWideChars
**                      included a terminating character for the conversion).
*/
int w2a_buffer(const unsigned short* inWideString, int inWideChars, char* outACPString, int inACPChars);

/*
**  Perform the requested conversion using heap memory.
**  The caller/client of this function must use free() to release the
**      resultant string to the heap once finished with said string.
**  This function is best used when the conversion length of inWideString
**      is not known beforehand.
**
**  inWideString    The wide character string to convert.
**  inWideChars     Count of wide characters (not bytes) in
**                      inWideString to be converted.
**                  If -1, assume a terminated string and the terminating
**                      character will also be appended to the return value.
**  outACPChars     Optional argument, can be NULL.
**                  Holds number of characters written into return value.
**                  Generally you would use outACPChars - 1 to avoid a
**                      strlen() call after the conversion took place
**                      should the string be terminated (i.e. if inWideChars
**                      included a terminating character for the conversion).
**  returns LPSTR   The malloced converted string which must eventually be
**                      free()d.
**                  NULL on failure.
*/

char* w2a_malloc(unsigned short* inWideString, int inWideChars, int* outACPChars);


void dumpMemoryInfo();

#define charcount(array) (sizeof(array) / sizeof(array[0]))


// We use this API internally as well as externally.
#ifdef __cplusplus
extern "C" {
#endif

	MOZCE_SHUNT_API int mozce_printf(const char *, ...);

#ifdef __cplusplus
};
#endif

int nclog (const char *fmt, ...);
void nclograw(const char* data, long length);

//#define MOZCE_PRECHECK                                                     \
//{                                                                          \
//    MEMORYSTATUS memStats;                                                 \
//    memStats.dwLength = sizeof( memStats);                                 \
//                                                                           \
//    GlobalMemoryStatus( (LPMEMORYSTATUS)&memStats );                       \
//                                                                           \
//    char buffer[100];                                                      \
//    sprintf(buffer, ">> dwAvailPhys (%d)\n", memStats.dwAvailPhys / 1024); \
//                                                                           \
//    nclograw(buffer, strlen(buffer));                                      \
//}                                                                          \

#define MOZCE_PRECHECK

#endif /* __mozce_internal_h */
