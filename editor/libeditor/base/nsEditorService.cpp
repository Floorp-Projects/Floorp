/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Seth Spitzer <sspitzer@netscape.com>
 */

#include "nsEditorService.h"

nsEditorService::nsEditorService()
{
    NS_INIT_REFCNT();
}

nsEditorService::~nsEditorService()
{
}

NS_IMPL_ADDREF(nsEditorService);
NS_IMPL_RELEASE(nsEditorService);
NS_IMPL_QUERY_INTERFACE2(nsEditorService,
                         nsIEditorService,
                         nsICmdLineHandler) 

CMDLINEHANDLER_IMPL(nsEditorService,"-edit","general.startup.editor","chrome://editor/content/editor.xul","Start with editor.","@mozilla.org/commandlinehandler/general-startup;1?type=editor","Editor Startup Handler", PR_TRUE,"about:blank", PR_TRUE)
