/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 */

#include "nsIGenericFactory.h"

#include "nsEditorShell.h"		      // for the CID
#include "nsComposerController.h"		// for the CID

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComposerController)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
    { "Composer Controller", NS_COMPOSERCONTROLLER_CID,
      "@mozilla.org/editor/composercontroller;1",
      nsComposerControllerConstructor, },
    { "Editor Shell Component", NS_EDITORSHELL_CID,
      "@mozilla.org/editor/editorshell;1", nsEditorShellConstructor, },
    { "Editor Shell Spell Checker", NS_EDITORSHELL_CID,
      "@mozilla.org/editor/editorspellcheck;1", nsEditorShellConstructor, },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE("nsComposerModule", components)
