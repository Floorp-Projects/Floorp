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
 *   Michael Judge  <mjudge@netscape.com>
 *   Charles Manske <cmanske@netscape.com>
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

#include "nsEditingSession.h"       // for the CID
#include "nsComposerController.h"   // for the CID
#include "nsEditorSpellCheck.h"     // for the CID
#include "nsComposeTxtSrvFilter.h"
#include "nsIController.h"
#include "nsIControllerContext.h"
#include "nsIControllerCommandTable.h"

#include "nsIServiceManagerUtils.h"

#define NS_HTMLEDITOR_COMMANDTABLE_CID \
{ 0x7a727843, 0x6ae1, 0x11d7, { 0xa5eb, 0x00, 0x03, 0x93, 0x63, 0x65, 0x92 } }

#define NS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID \
{ 0x800e07bc, 0x6ae1, 0x11d7, { 0x959b, 0x00, 0x03, 0x93, 0x63, 0x65, 0x92 } }


static NS_DEFINE_CID(kHTMLEditorCommandTableCID, NS_HTMLEDITOR_COMMANDTABLE_CID);
static NS_DEFINE_CID(kHTMLEditorDocStateCommandTableCID, NS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID);


////////////////////////////////////////////////////////////////////////
// Define the contructor function for the objects
//
// NOTE: This creates an instance of objects by using the default constructor
//

NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditingSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEditorSpellCheck)

// There are no macros that enable us to have 2 constructors 
// for the same object
//
// Here we are creating the same object with two different contract IDs
// and then initializing it different.
// Basically, we need to tell the filter whether it is doing mail or not
static NS_METHOD
nsComposeTxtSrvFilterConstructor(nsISupports *aOuter, REFNSIID aIID,
                                 void **aResult, PRBool aIsForMail)
{
    *aResult = NULL;
    if (NULL != aOuter) 
    {
        return NS_ERROR_NO_AGGREGATION;
    }
    nsComposeTxtSrvFilter * inst;
    NS_NEWXPCOM(inst, nsComposeTxtSrvFilter);
    if (NULL == inst) 
    {
        return NS_ERROR_OUT_OF_MEMORY;
    }
    NS_ADDREF(inst);
	  inst->Init(aIsForMail);
    nsresult rv = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
    return rv;
}

static NS_METHOD
nsComposeTxtSrvFilterConstructorForComposer(nsISupports *aOuter, 
                                            REFNSIID aIID,
                                            void **aResult)
{
    return nsComposeTxtSrvFilterConstructor(aOuter, aIID, aResult, PR_FALSE);
}

static NS_METHOD
nsComposeTxtSrvFilterConstructorForMail(nsISupports *aOuter, 
                                        REFNSIID aIID,
                                        void **aResult)
{
    return nsComposeTxtSrvFilterConstructor(aOuter, aIID, aResult, PR_TRUE);
}


// Constructor for a controller set up with a command table specified
// by the CID passed in. This function uses do_GetService to get the
// command table, so that every controller shares a single command
// table, for space-efficiency.
// 
// The only reason to go via the service manager for the command table
// is that it holds onto the singleton for us, avoiding static variables here.
static nsresult
CreateControllerWithSingletonCommandTable(const nsCID& inCommandTableCID, nsIController **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIController> controller = do_CreateInstance("@mozilla.org/embedcomp/base-command-controller;1", &rv);
  if (NS_FAILED(rv)) return rv;

  nsCOMPtr<nsIControllerCommandTable> composerCommandTable = do_GetService(inCommandTableCID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  // this guy is a singleton, so make it immutable
  composerCommandTable->MakeImmutable();
  
  nsCOMPtr<nsIControllerContext> controllerContext = do_QueryInterface(controller, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = controllerContext->Init(composerCommandTable);
  if (NS_FAILED(rv)) return rv;
  
  *aResult = controller;
  NS_ADDREF(*aResult);
  return NS_OK;
}


// Here we make an instance of the controller that holds doc state commands.
// We set it up with a singleton command table.
static NS_METHOD
nsHTMLEditorDocStateControllerConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = CreateControllerWithSingletonCommandTable(kHTMLEditorDocStateCommandTableCID, getter_AddRefs(controller));
  if (NS_FAILED(rv)) return rv;

  return controller->QueryInterface(aIID, aResult);
}

// Tere we make an instance of the controller that holds composer commands.
// We set it up with a singleton command table.
static NS_METHOD
nsHTMLEditorControllerConstructor(nsISupports *aOuter, REFNSIID aIID, void **aResult)
{
  nsCOMPtr<nsIController> controller;
  nsresult rv = CreateControllerWithSingletonCommandTable(kHTMLEditorCommandTableCID, getter_AddRefs(controller));
  if (NS_FAILED(rv)) return rv;

  return controller->QueryInterface(aIID, aResult);
}

// Constructor for a command table that is pref-filled with HTML editor commands
static NS_METHOD
nsHTMLEditorCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = nsComposerController::RegisterHTMLEditorCommands(commandTable);
  if (NS_FAILED(rv)) return rv;
  
  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable
  
  return commandTable->QueryInterface(aIID, aResult);
}


// Constructor for a command table that is pref-filled with HTML editor doc state commands
static NS_METHOD
nsHTMLEditorDocStateCommandTableConstructor(nsISupports *aOuter, REFNSIID aIID, 
                                              void **aResult)
{
  nsresult rv;
  nsCOMPtr<nsIControllerCommandTable> commandTable =
      do_CreateInstance(NS_CONTROLLERCOMMANDTABLE_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  
  rv = nsComposerController::RegisterEditorDocStateCommands(commandTable);
  if (NS_FAILED(rv)) return rv;
  
  // we don't know here whether we're being created as an instance,
  // or a service, so we can't become immutable
  
  return commandTable->QueryInterface(aIID, aResult);
}

////////////////////////////////////////////////////////////////////////
// Define a table of CIDs implemented by this module along with other
// information like the function to create an instance, contractid, and
// class name.
//
static const nsModuleComponentInfo components[] = {

    { "HTML Editor Controller", NS_HTMLEDITORCONTROLLER_CID,
      "@mozilla.org/editor/htmleditorcontroller;1",
      nsHTMLEditorControllerConstructor, },

    { "HTML Editor DocState Controller", NS_EDITORDOCSTATECONTROLLER_CID,
      "@mozilla.org/editor/editordocstatecontroller;1",
      nsHTMLEditorDocStateControllerConstructor, },

    { "HTML Editor command table", NS_HTMLEDITOR_COMMANDTABLE_CID,
      "", // no point using a contract-ID
      nsHTMLEditorCommandTableConstructor, },

    { "HTML Editor doc state command table", NS_HTMLEDITOR_DOCSTATE_COMMANDTABLE_CID,
      "", // no point using a contract-ID
      nsHTMLEditorDocStateCommandTableConstructor, },

    { "Editing Session", NS_EDITINGSESSION_CID,
      "@mozilla.org/editor/editingsession;1", nsEditingSessionConstructor, },

    { "Editor Spell Checker", NS_EDITORSPELLCHECK_CID,
      "@mozilla.org/editor/editorspellchecker;1",
      nsEditorSpellCheckConstructor,},

    { "TxtSrv Filter", NS_COMPOSERTXTSRVFILTER_CID,
      COMPOSER_TXTSRVFILTER_CONTRACTID,
      nsComposeTxtSrvFilterConstructorForComposer, },

    { "TxtSrv Filter For Mail", NS_COMPOSERTXTSRVFILTERMAIL_CID,
      COMPOSER_TXTSRVFILTERMAIL_CONTRACTID,
      nsComposeTxtSrvFilterConstructorForMail, },
};

////////////////////////////////////////////////////////////////////////
// Implement the NSGetModule() exported function for your module
// and the entire implementation of the module object.
//
NS_IMPL_NSGETMODULE(nsComposerModule, components)
