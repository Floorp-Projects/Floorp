/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is RaptorCanvas.
 *
 * The Initial Developer of the Original Code is Kirk Baker and
 * Ian Wilkinson. Portions created by Kirk Baker and Ian Wilkinson are
 * Copyright (C) 1999 Kirk Baker and Ian Wilkinson. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns <edburns@acm.org>
 *
 */


/**

 * Methods used in the jni_util_export implementation, but not exported
 * from webclient.dll.

 */

#ifndef bal_util_h
#define bal_util_h

#ifdef __cplusplus
extern "C" {
#endif

#include <jni.h>

#define bnull 0

void JNICALL bal_jstring_newFromAscii(jstring *newStr, const char *value);

jint JNICALL bal_str_getLength(const char *str);

jint JNICALL bal_jstring_getLength(const jstring str);

void JNICALL bal_str_newFromJstring(char **newStr, const jstring value);

void JNICALL bal_jstring_release(jstring value);

void JNICALL bal_str_release(const char *str);

void * JNICALL bal_allocateMemory(jint bytes);

void JNICALL bal_freeMemory(void *MemA);

void * JNICALL bal_findInMemory(const void *MemA, 
                                jchar ch, 
                                jint bytes);

jstring JNICALL bal_findInJstring(const jstring MemA, 
                                  jchar ch);
 

#ifdef __cplusplus
} /* extern "C" */
#endif /* __cplusplus */

#endif // bal_util_h
