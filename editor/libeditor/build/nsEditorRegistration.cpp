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
#include "nsEditor.h"				// for gInstanceCount
#include "nsEditorService.h" 
#include "nsHTMLEditor.h"
#include "nsPlaintextEditor.h"

#include "nsEditorController.h" //CID

#define DO_COMPOSER_TOO 1
#ifdef DO_COMPOSER_TOO
#include "nsComposerController.h" //CID
#include "nsEditorShell.h"		// for the CID
#endif /* DO_COMPOSER_TOO */


////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPlaintextEditor)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorController)
#ifdef DO_COMPOSER_TOO
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComposerController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorShell)
#endif /* DO_COMPOSER_TOO */

#ifdef ENABLE_EDITOR_API_LOG
#include "nsHTMLEditorLog.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditorLog)
#else
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditor)
#endif

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static nsModuleComponentInfo components[] = {
    { "Text Editor", NS_TEXTEDITOR_CID,
      "@mozilla.org/editor/texteditor;1", nsPlaintextEditorConstructor, },
#ifdef ENABLE_EDITOR_API_LOG
    { "HTML Editor", NS_HTMLEDITOR_CID,
      "@mozilla.org/editor/htmleditor;1", nsHTMLEditorLogConstructor, },
#else
    { "HTML Editor", NS_HTMLEDITOR_CID,
      "@mozilla.org/editor/htmleditor;1", nsHTMLEditorConstructor, },
#endif
    { "Editor Service", NS_EDITORSERVICE_CID,
      "@mozilla.org/editor/editorservice;1", nsEditorServiceConstructor,},
    { "Editor Startup Handler", NS_EDITORSERVICE_CID,
      "@mozilla.org/commandlinehandler/general-startup;1?type=editor",
      nsEditorServiceConstructor,
      nsEditorService::RegisterProc,
      nsEditorService::UnregisterProc, },
    { "Edit Startup Handler", NS_EDITORSERVICE_CID,
      "@mozilla.org/commandlinehandler/general-startup;1?type=edit",
      nsEditorServiceConstructor, },
    { "Editor Controller", NS_EDITORCONTROLLER_CID,
      "@mozilla.org/editor/editorcontroller;1",
      nsEditorControllerConstructor, },
#ifdef DO_COMPOSER_TOO
    { "Composer Controller", NS_COMPOSERCONTROLLER_CID,
      "@mozilla.org/editor/composercontroller;1",
      nsComposerControllerConstructor, },
    { "Editor Shell Component", NS_EDITORSHELL_CID,
      "@mozilla.org/editor/editorshell;1", nsEditorShellConstructor, },
    { "Editor Shell Spell Checker", NS_EDITORSHELL_CID,
      "@mozilla.org/editor/editorspellcheck;1", nsEditorShellConstructor, },
#endif /* DO_COMPOSER_TOO */
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(nsEditorModule, components)
