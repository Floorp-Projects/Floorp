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
#include "nsEditorShell.h"		// for the CID
#include "nsEditor.h"				// for gInstanceCount
#include "nsEditorController.h" //CID
#include "nsEditorService.h" 

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorShell)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComposerController)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorService)

#ifdef ENABLE_EDITOR_API_LOG
#include "nsHTMLEditorLog.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditorLog)
#else
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditor)
#endif

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, progid, and
// class name.
//
static nsModuleComponentInfo components[] = {
#ifdef ENABLE_EDITOR_API_LOG
    { "HTML Editor", NS_HTMLEDITOR_CID,
      "component://netscape/editor/htmleditor", nsHTMLEditorLogConstructor, },
#else
    { "HTML Editor", NS_HTMLEDITOR_CID,
      "component://netscape/editor/htmleditor", nsHTMLEditorConstructor, },
#endif
    { "Editor Controller", NS_EDITORCONTROLLER_CID,
      "component://netscape/editor/editorcontroller", nsEditorControllerConstructor, },
    { "Composer Controller", NS_COMPOSERCONTROLLER_CID,
      "component://netscape/editor/composercontroller", nsComposerControllerConstructor, },
    { "Editor Shell Component", NS_EDITORSHELL_CID,
      "component://netscape/editor/editorshell", nsEditorShellConstructor, },
    { "Editor Shell Spell Checker", NS_EDITORSHELL_CID,
      "component://netscape/editor/editorspellcheck", nsEditorShellConstructor, },
    { "Editor Service", NS_EDITORSERVICE_CID,
      "component://netscape/editor/editorservice", nsEditorServiceConstructor,},
    { "Editor Startup Handler", NS_EDITORSERVICE_CID,
      "component://netscape/commandlinehandler/general-startup-editor",
      nsEditorServiceConstructor,
      nsEditorService::RegisterProc,
      nsEditorService::UnregisterProc, },
    { "Edit Startup Handler", NS_EDITORSERVICE_CID,
      "component://netscape/commandlinehandler/general-startup-edit",
      nsEditorServiceConstructor, },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE("nsEditorModule", components)
