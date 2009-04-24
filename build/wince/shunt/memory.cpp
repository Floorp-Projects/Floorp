/* -*- Mode: C;  c-basic-offset: 2; tab-width: 4; indent-tabs-mode: nil; -*- */
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

#include "include/mozce_shunt.h"
#include <stdlib.h>

#ifdef MOZ_MEMORY
void * operator new(size_t _Size)
{
   void *p =  moz_malloc(_Size);
   return (p);
}

void operator delete(void * ptr)
{
  moz_free(ptr);  
}
void *operator new[](size_t size)
{
  void* p = moz_malloc(size);
  return (p);
}
void operator delete[](void *ptr)
{
  moz_free(ptr);
}

char*
mozce_strndup( const char *src, size_t len ) {
  char* dst = (char*)moz_malloc(len + 1);
  if(dst)
    strncpy(dst, src, len + 1);
  return dst;
}


char*
mozce_strdup(const char *src ) {
  size_t len = strlen(src);
  return mozce_strndup(src, len );
}

unsigned short* 
mozce_wcsndup( const unsigned short *src, size_t len ) {
  wchar_t* dst = (wchar_t*)moz_malloc(sizeof(wchar_t) * (len + 1));
  if(dst)
    wcsncpy(dst, src, len + 1);
  return dst;
}

unsigned short* 
mozce_wcsdup( const unsigned short *src ) {
  size_t len = wcslen(src);
  return mozce_wcsndup(src, len);
}
void* __cdecl malloc(size_t size) {
  return moz_malloc(size);
}
void* __cdecl valloc(size_t size) {
  return moz_valloc(size);
}
void* __cdecl calloc(size_t size, size_t num) {
  return moz_calloc(size, num);
}
void* __cdecl realloc(void* ptr, size_t size) {
  return moz_realloc(ptr, size);
}
void __cdecl free(void* ptr) {
  return moz_free(ptr);
}
#endif

