/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is MOZCE Lib.
 *
 * The Initial Developer of the Original Code is Doug Turner <dougt@meer.net>.

 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  John Wolfe <wolfe@lobo.us>
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


#ifndef MOZCE_SHUNT_H
#define MOZCE_SHUNT_H

#ifdef MOZCE_SHUNT_EXPORTS
#define _CRTIMP __declspec(dllexport)
#endif

#ifdef MOZ_MEMORY
#ifdef __cplusplus


#define _NEW_
void * operator new(size_t _Size);
void operator delete(void * ptr);
void *operator new[](size_t size);
void operator delete[](void *ptr);

extern "C" {
#endif

extern void* moz_malloc(size_t);
extern void* moz_valloc(size_t);
extern void* moz_calloc(size_t, size_t);
extern void* moz_realloc(void*, unsigned int);
extern void moz_free(void*);
  
void* __cdecl  malloc(size_t);
void* __cdecl  valloc(size_t);
void* __cdecl  calloc(size_t, size_t);
void* __cdecl  realloc(void*, unsigned int);
void __cdecl  free(void*);

 
char*
mozce_strdup(const char*);

MOZCE_SHUNT_API unsigned short* 
mozce_wcsdup(const unsigned short* );

MOZCE_SHUNT_API char*
mozce_strndup(const char *, unsigned int);

MOZCE_SHUNT_API unsigned short* 
mozce_wcsndup(const unsigned short*, unsigned int);
  
#ifdef __cplusplus
}   //extern "C" 
#endif


#undef _strdup
#undef strdup
#undef _strndup
#undef strndup
#undef _wcsdup
#undef wcsdup
#undef _wcsndup
#undef wcsndup



#define _strdup mozce_strdup
#define _strndup mozce_strndup

#define _wcsdup mozce_wcsdup
#define _wcsndup mozce_wcsndup

#endif

#define strdup  _strdup
#define strndup _strndup
#define wcsdup _wcsdup
#define wcsndup _wcsndup


#define strcmpi _stricmp
#define stricmp _stricmp
#define wgetcwd _wgetcwd
#define vsnprintf _vsnprintf

#define SHGetSpecialFolderPathW SHGetSpecialFolderPath
#define SHGetPathFromIDListW    SHGetPathFromIDList
#define FONTENUMPROCW           FONTENUMPROC

#ifdef __cplusplus
extern "C" {
#endif

/* errno and family */
extern MOZCE_SHUNT_IMPORT_API int errno;
MOZCE_SHUNT_IMPORT_API char* strerror(int);

/* abort */
MOZCE_SHUNT_API void abort(void);
  
/* Environment stuff */
MOZCE_SHUNT_API char* getenv(const char* inName);
MOZCE_SHUNT_API int putenv(const char *a);
MOZCE_SHUNT_API char SetEnvironmentVariableW(const unsigned short * name, const unsigned short * value );
MOZCE_SHUNT_API char GetEnvironmentVariableW(const unsigned short * lpName, unsigned short* lpBuffer, unsigned long nSize);
  
MOZCE_SHUNT_API unsigned int ExpandEnvironmentStringsW(const unsigned short* lpSrc,
                                                       unsigned short* lpDst,
                                                       unsigned int nSize);

/* File system stuff */
MOZCE_SHUNT_API unsigned short * _wgetcwd(unsigned short* dir, unsigned long size);
MOZCE_SHUNT_API unsigned short *_wfullpath( unsigned short *absPath, const unsigned short *relPath, unsigned long maxLength );
MOZCE_SHUNT_API int _unlink(const char *filename );
  
/* The time stuff should be defined here, but it can't be because it
   is already defined in time.h.
  
 MOZCE_SHUNT_API size_t strftime(char *, size_t, const char *, const struct tm *)
 MOZCE_SHUNT_API struct tm* localtime(const time_t* inTimeT)
 MOZCE_SHUNT_API struct tm* mozce_gmtime_r(const time_t* inTimeT, struct tm* outRetval)
 MOZCE_SHUNT_API struct tm* gmtime(const time_t* inTimeT)
 MOZCE_SHUNT_API time_t mktime(struct tm* inTM)
 MOZCE_SHUNT_API time_t time(time_t *)
 MOZCE_SHUNT_API clock_t clock() 
  
*/
  
/* Locale Stuff */
  
/* The locale stuff should be defined here, but it can't be because it
   is already defined in locale.h.
  
 MOZCE_SHUNT_API struct lconv * localeconv(void)
  
*/


MOZCE_SHUNT_API unsigned short* mozce_GetEnvironmentCL();

  /* square root of 1/2, missing from math.h */ 
#define M_SQRT1_2  0.707106781186547524401

#ifdef __cplusplus
};
#endif

#endif //MOZCE_SHUNT_H
