/******* BEGIN LICENSE BLOCK *******
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
 * The Initial Developers of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developers
 * are Copyright (C) 2011 the Initial Developers. All Rights Reserved.
 * 
 * Contributor(s):
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
 ******* END LICENSE BLOCK *******/

#ifndef alloc_hooks_h__
#define alloc_hooks_h__

/**
 * This file is force-included in hunspell code.  Its purpose is to add memory
 * reporting to hunspell without modifying its code, in order to ease future
 * upgrades.
 *
 * This file is force-included through mozilla-config.h which is generated
 * during the configure step.
 *
 * Currently, the memory allocated using operator new/new[] is not being
 * tracked, but that's OK, since almost all of the memory used by Hunspell is
 * allocated using C memory allocation functions.
 */

// Prevent the standard macros from being redefined
#define mozilla_mozalloc_macro_wrappers_h

#include "mozilla/mozalloc.h"

extern void HunspellReportMemoryAllocation(void*);
extern void HunspellReportMemoryDeallocation(void*);

inline void* hunspell_malloc(size_t size)
{
  void* result = moz_malloc(size);
  HunspellReportMemoryAllocation(result);
  return result;
}
#define malloc(size) hunspell_malloc(size)

inline void* hunspell_calloc(size_t count, size_t size)
{
  void* result = moz_calloc(count, size);
  HunspellReportMemoryAllocation(result);
  return result;
}
#define calloc(count, size) hunspell_calloc(count, size)

inline void hunspell_free(void* ptr)
{
  HunspellReportMemoryDeallocation(ptr);
  moz_free(ptr);
}
#define free(ptr) hunspell_free(ptr)

inline void* hunspell_realloc(void* ptr, size_t size)
{
  HunspellReportMemoryDeallocation(ptr);
  void* result = moz_realloc(ptr, size);
  HunspellReportMemoryAllocation(result);
  return result;
}
#define realloc(ptr, size) hunspell_realloc(ptr, size)

inline char* hunspell_strdup(const char* str)
{
  char* result = moz_strdup(str);
  HunspellReportMemoryAllocation(result);
  return result;
}
#define strdup(str) hunspell_strdup(str)

#if defined(HAVE_STRNDUP)
inline char* hunspell_strndup(const char* str, size_t size)
{
  char* result = moz_strndup(str, size);
  HunspellReportMemoryAllocation(result);
  return result;
}
#define strndup(str, size) hunspell_strndup(str, size)
#endif

#endif
