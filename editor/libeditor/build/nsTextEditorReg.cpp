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

#include "nsEditorCID.h"
#include "nsEditor.h"           // for gInstanceCount
#include "nsPlaintextEditor.h"
#include "nsEditorService.h" 
#include "nsEditorController.h" //CID

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorService)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPlaintextEditor)

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
    { "Text Editor", NS_TEXTEDITOR_CID,
      "@mozilla.org/editor/texteditor;1", nsPlaintextEditorConstructor, },
    { "Editor Controller", NS_EDITORCONTROLLER_CID,
      "@mozilla.org/editor/editorcontroller;1", nsEditorControllerConstructor, }
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(nsEditorModule, components)
