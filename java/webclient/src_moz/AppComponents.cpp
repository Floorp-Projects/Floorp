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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems, Inc.
 * Portions created by Sun Microsystems are Copyright (C) 2003 Sun
 * Microsystems, Inc. All Rights Reserved.
 *
 * Original Author: Kyle Yuan <kyle.yuan@sun.com>
 *
 * Contributor(s):
 *
 */

#include "AppComponents.h"
#include "PromptService.h"

#define NS_PROMPTSERVICE_CID \
    {0xa2112d6a, 0x0e28, 0x421f, {0xb4, 0x6a, 0x25, 0xc0, 0xb3, 0x8, 0xcb, 0xd0}}

NS_GENERIC_FACTORY_CONSTRUCTOR(PromptService);

static const nsModuleComponentInfo components[] = {
    {
        "Prompt Service",
        NS_PROMPTSERVICE_CID,
        "@mozilla.org/embedcomp/prompt-service;1",
        PromptServiceConstructor
    }
};

const nsModuleComponentInfo* GetAppModuleComponentInfo(int* outNumComponents)
{
    *outNumComponents = sizeof(components) / sizeof(components[0]);
    return components;
}
