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

#include "jni_util_export.h" // from ../src_share

#include "nsHashtable.h" // PENDING(edburns): size optimization?

static nsHashtable *gClassMappingTable = nsnull;

//
// Implementations for functions defined in ../src_share/jni_util.h, but not
// implemented there.
// 

JNIEXPORT jint JNICALL util_StoreClassMapping(const char* jniClassName,
                                              jclass yourClassType)
{
    if (nsnull == gClassMappingTable) {
        if (nsnull == (gClassMappingTable = new nsHashtable(20, PR_TRUE))) {
            return -1;
        }
    }

    NS_ConvertASCIItoUCS2 keyString(jniClassName);
    nsStringKey key(keyString);
    gClassMappingTable->Put(&key, yourClassType);

    return 0;
}

JNIEXPORT jclass JNICALL util_GetClassMapping(const char* jniClassName)
{
    jclass result = nsnull;

    if (nsnull == gClassMappingTable) {
        return nsnull;
    }

    NS_ConvertASCIItoUCS2 keyString(jniClassName);
    nsStringKey key(keyString);
    result = (jclass) gClassMappingTable->Get(&key);
    
    return result;
}
