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


#include <stdio.h>

#include "WrapperFactoryImpl.h"

#include "jni_util_export.h"

int main(int argc, char **argv)
{
    jstring result = NULL;
    jboolean doesImplement;

    if (2 > argc) {
        printf("usage: bal_test <absolute path to mozilla bin dir>\n");
        return -1;
    }

    result = util_NewStringUTF(NULL, argv[1]);

    Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeAppInitialize(NULL, NULL, result);

    util_DeleteStringUTF(NULL, result);

    result = util_NewStringUTF(NULL, "webclient.WindowControl");

    doesImplement = 
        Java_org_mozilla_webclient_wrapper_1native_WrapperFactoryImpl_nativeDoesImplement(NULL, NULL, result);

    printf("doesImplement: %d\n", doesImplement);

    util_DeleteStringUTF(NULL, result);

    return 0;
}
