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

 * Implementation for bal_util functions.

 */

#include "malloc.h"
#include "string.h"

#include "bal_util.h"

void JNICALL bal_jstring_newFromAscii(jstring *newStr, const char *value)
{
	jint length;
	jint i;
	jstring p;

    if (bnull == newStr) {
        return;
    }
    
	length = bal_str_getLength(value);
    
	*newStr = (jstring)bal_allocateMemory(sizeof(jchar) + 
                                          (length * sizeof(jchar)));
    
	if (!(*newStr)) return;
    
	p = *newStr;
	for (i = 0; i < length; i++) {
        /* Check ASCII range */
        // OSL_ENSHURE( (*value & 0x80) == 0, "Found ASCII char > 127");
        
        *(p++) = (jchar)*(value++);
    }
	*p = 0;
}

void JNICALL bal_str_newFromJstring(char **newStr, const jstring inValue)
{
	jint length;
	jint i;
	char *p;
    jstring value = inValue;

    if (bnull == newStr) {
        return;
    }
    
	length = bal_jstring_getLength(value);
    
	*newStr = (char *)bal_allocateMemory(sizeof(char *) + 
                                         (length * sizeof(char *)));
    
	if (!(*newStr)) return;
    
	p = *newStr;
	for (i = 0; i < length; i++) {
        /* Check ASCII range */
        // OSL_ENSHURE( (*value & 0x80) == 0, "Found ASCII char > 127");
        
        *(p++) = (char)*(value++);
    }
	*p = 0;
}

jint JNICALL bal_str_getLength(const char *str)
{
    jint result = -1;

    const char * pTempStr = (char *)bal_findInMemory(str, '\0', 
                                                     0x80000000);
    result = pTempStr - str;


    return result;
}

jint JNICALL bal_jstring_getLength(const jstring str)
{
    jint result = -1;
    
    const jstring pTempStr = bal_findInJstring(str, '\0');
    result = pTempStr - str;
    return result;
}

void JNICALL bal_jstring_release(jstring value)
{
    bal_freeMemory(value);
}

void JNICALL bal_str_release(const char *str)
{
    bal_freeMemory((void *)str);
}

void * JNICALL bal_allocateMemory(jint bytes)
{
    void *result = bnull;

    if (0 < bytes) {
        result = malloc(bytes);
    }

    return result;
}

void JNICALL bal_freeMemory(void *MemA)
{
    free(MemA);
    MemA = bnull;
}

void * JNICALL bal_findInMemory(const void *MemA, 
                                jchar ch, 
                                jint bytes)
{
    void *result = bnull;

    result = memchr(MemA, ch, bytes);
    
    return result;
}

jstring JNICALL bal_findInJstring(const jstring MemA, 
                                  jchar ch)
{
    jstring result = bnull;

    result = wcschr(MemA, ch);

    return result;
}
