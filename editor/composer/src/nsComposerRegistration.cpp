/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Judge  <mjudge@netscape.com>
 *   Charles Manske <cmanske@netscape.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"

#include "nsEditingSession.h"       // for the CID
#include "nsComposerController.h"   // for the CID
#include "nsEditorSpellCheck.h"     // for the CID
#include "nsEditorService.h"
#include "nsIControllerContext.h"

////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditingSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorSpellCheck)


NS_IMETHODIMP nsEditorDocStateControllerConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  static PRBool sDocStateCommandsRegistered = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIControllerContext> context =
      do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  if (!context)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIControllerCommandManager> composerCommandManager(
      do_GetService(NS_COMPOSERSCONTROLLERCOMMANDMANAGER_CONTRACTID, &rv));

  if (NS_FAILED(rv))
    return rv;
  if (!composerCommandManager)
    return NS_ERROR_OUT_OF_MEMORY;
  if (!sDocStateCommandsRegistered)
  {
    rv = nsComposerController::RegisterEditorDocStateCommands(composerCommandManager);
    if (NS_FAILED(rv))
    {
      return rv;
    }
    sDocStateCommandsRegistered = PR_TRUE;
  }
  
  context->SetControllerCommandManager(composerCommandManager);
  return context->QueryInterface(aIID, aResult);
}

NS_IMETHODIMP nsHTMLEditorControllerConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  static PRBool sHTMLCommandsRegistered = PR_FALSE;

  nsresult rv;
  nsCOMPtr<nsIControllerContext> context =
      do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  if (NS_FAILED(rv))
    return rv;
  if (!context)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIControllerCommandManager> composerCommandManager(
      do_GetService(NS_COMPOSERSCONTROLLERCOMMANDMANAGER_CONTRACTID, &rv));
  if (NS_FAILED(rv))
    return rv;
  if (!composerCommandManager)
    return NS_ERROR_OUT_OF_MEMORY;
  if (!sHTMLCommandsRegistered)
  {
    rv = nsComposerController::RegisterHTMLEditorCommands(composerCommandManager);
    if (NS_FAILED(rv))
    {
      return rv;
    }
    sHTMLCommandsRegistered = PR_TRUE;
  }
  
  context->SetControllerCommandManager(composerCommandManager);
  return context->QueryInterface(aIID, aResult);
}

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo components[] = {
    { "Editor DocState Controller", NS_EDITORDOCSTATECONTROLLER_CID,
      "@mozilla.org/editor/editordocstatecontroller;1",
      nsEditorDocStateControllerConstructor, },
    { "HTML Editor Controller", NS_HTMLEDITORCONTROLLER_CID,
      "@mozilla.org/editor/htmleditorcontroller;1",
      nsHTMLEditorControllerConstructor, },
    { "Editing Session", NS_EDITINGSESSION_CID,
      "@mozilla.org/editor/editingsession;1", nsEditingSessionConstructor, },
    { "Editor Service", NS_EDITORSERVICE_CID,
      "@mozilla.org/editor/editorservice;1", nsEditorServiceConstructor,},
    { "Editor Spell Checker", NS_EDITORSPELLCHECK_CID,
      "@mozilla.org/editor/editorspellchecker;1",
      nsEditorSpellCheckConstructor,},
    { "Editor Startup Handler", NS_EDITORSERVICE_CID,
      "@mozilla.org/commandlinehandler/general-startup;1?type=editor",
      nsEditorServiceConstructor,
      nsEditorService::RegisterProc,
      nsEditorService::UnregisterProc, },
    { "Edit Startup Handler", NS_EDITORSERVICE_CID,
      "@mozilla.org/commandlinehandler/general-startup;1?type=edit",
      nsEditorServiceConstructor, },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(nsComposerModule, components)
