/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 *   Michael Judge   <mjudge@netscape.com>
 *   Charles Manske  <cmanske@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIGenericFactory.h"
#include "nsEditorCID.h"
#include "nsEditor.h"				// for gInstanceCount
#include "nsPlaintextEditor.h"
#include "nsEditorController.h" //CID
#include "nsIController.h"
#include "nsIControllerContext.h"
#include "nsIControllerCommandTable.h"
#include "nsIServiceManager.h"
#include "nsIObserver.h"
#include "nsIObserverService.h"
#include "nsCOMPtr.h"

#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
#include "nsHTMLEditor.h"
#include "nsTextServicesDocument.h"
#include "nsTextServicesCID.h"
#endif

#define NS_EDITORCOMMANDTABLE_CID \
{ 0x8b975b0a, 0x6ae5, 0x11d7, { 0xa44c, 0x00, 0x03, 0x93, 0x63, 0x65, 0x92 } }

static NS_DEFINE_CID(kEditorCommandTableCID, NS_EDITORCOMMANDTABLE_CID);

PR_STATIC_CALLBACK(nsresult) Initialize(nsIModule* self);
static void Shutdown();

class EditorShutdownObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER
};

NS_IMPL_ISUPPORTS1(EditorShutdownObserver, nsIObserver)

NS_IMETHODIMP
EditorShutdownObserver::Observe(nsISupports *aSubject,
                                const char *aTopic,
                                const PRUnichar *someData)
{
  if (!strcmp(aTopic, NS_XPCOM_SHUTDOWN_OBSERVER_ID))
    Shutdown();
  return NS_OK;
}

static PRBool gInitialized = PR_FALSE;

PR_STATIC_CALLBACK(nsresult)
Initialize(nsIModule* self)
{
  NS_PRECONDITION(!gInitialized, "module already initialized");
  if (gInitialized) {
    return NS_OK;
  }

  gInitialized = PR_TRUE;

#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
  nsEditProperty::RegisterAtoms();
  nsTextServicesDocument::RegisterAtoms();
#endif

  // Add our shutdown observer.
  nsCOMPtr<nsIObserverService> observerService =
    do_GetService("@mozilla.org/observer-service;1");

  if (observerService) {
    nsCOMPtr<EditorShutdownObserver> observer =
      new EditorShutdownObserver();

    if (!observer) {
      Shutdown();

      return NS_ERROR_OUT_OF_MEMORY;
    }

    observerService->AddObserver(observer, NS_XPCOM_SHUTDOWN_OBSERVER_ID, PR_FALSE);
  } else {
    NS_WARNING("Could not get an observer service.  We will leak on shutdown.");
  }

  return NS_OK;
}

// static
void
Shutdown()
{
  NS_PRECONDITION(gInitialized, "module not initialized");
  if (!gInitialized)
    return;

#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
  nsHTMLEditor::Shutdown();
  nsTextServicesDocument::Shutdown();
#endif
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsPlaintextEditor)


// Constructor of a controller which is set up to use, internally, a
// singleton command-table pre-filled with editor commands.
static NS_METHOD
nsEditorControllerConstructor(nsISupports *aOuter, REFNSIID aIID,
                                            void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIControllerCommandTable> editorCommandTable = do_GetService(kEditorCommandTableCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  // this guy is a singleton, so make it immutable
  editorCommandTable->MakeImmutable();
  
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = controllerContext->Init(editorCommandTable);
  if (NS_FAILED(rv)) return rv;
  
  return controller->QueryInterface(aIID, aResult);
}


// Constructor for a command-table pref-filled with editor commands
static NS_METHOD
nsEditorCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID,
                                            void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = nsEditorController::RegisterEditorCommands(commandTable);
  if (NS_FAILED(rv)) return rv;
  
  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable

  return commandTable->QueryInterface(aIID, aResult);
}


#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
NS_GENERIC_FACTORY_CONSTRUCTOR(nsTextServicesDocument)
#ifdef ENABLE_EDITOR_API_LOG
#include "nsHTMLEditorLog.h"
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditorLog)
#else
NS_GENERIC_FACTORY_CONSTRUCTOR(nsHTMLEditor)
#endif
#endif

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo components[] = {
    { "Text Editor", NS_TEXTEDITOR_CID,
      "@mozilla.org/editor/texteditor;1", nsPlaintextEditorConstructor, },

#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
#ifdef ENABLE_EDITOR_API_LOG
    { "HTML Editor", NS_HTMLEDITOR_CID,
      "@mozilla.org/editor/htmleditor;1", nsHTMLEditorLogConstructor, },
#else
    { "HTML Editor", NS_HTMLEDITOR_CID,
      "@mozilla.org/editor/htmleditor;1", nsHTMLEditorConstructor, },
#endif
#endif

    { "Editor Controller", NS_EDITORCONTROLLER_CID,
      "@mozilla.org/editor/editorcontroller;1",
      nsEditorControllerConstructor, },

    { "Editor Command Table", NS_EDITORCOMMANDTABLE_CID,
      "",   // no point in using a contract ID
      nsEditorCommandTableConstructor, },

#ifndef MOZILLA_PLAINTEXT_EDITOR_ONLY
    { NULL, NS_TEXTSERVICESDOCUMENT_CID,
      "@mozilla.org/textservices/textservicesdocument;1",
      nsTextServicesDocumentConstructor },
#endif
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE_WITH_CTOR(nsEditorModule, components, Initialize)
