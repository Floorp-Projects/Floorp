/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


#include "nsCOMPtr.h"

#include "nsIServiceManager.h"
#include "nsIComponentManager.h"

#include "nsEditorCID.h"

#include "nsEditorShell.h"		// for the CID
#include "nsEditorShellFactory.h"

#include "nsEditor.h"				// for gInstanceCount
#include "nsEditorFactory.h"

static NS_DEFINE_CID(kComponentManagerCID,  NS_COMPONENTMANAGER_CID);


static NS_DEFINE_IID(kHTMLEditorCID,        NS_HTMLEDITOR_CID);
static NS_DEFINE_IID(kEditorShellCID,       NS_EDITORSHELL_CID);


/*
we must be good providers of factories etc. this is where to put ALL editor exports
*/
//BEGIN EXPORTS
extern "C" NS_EXPORT nsresult NSGetFactory(nsISupports * aServMgr, 
                                           const nsCID & aClass, 
                                           const char * aClassName,
                                           const char * aProgID,
                                           nsIFactory ** aFactory)
{
  if (nsnull == aFactory) {
    return NS_ERROR_NULL_POINTER;
  }

  *aFactory = nsnull;

  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* componentManager = nsnull;
  rv = servMgr->GetService(kComponentManagerCID, nsIComponentManager::GetIID(), 
                         (nsISupports**)&componentManager);
  if (NS_FAILED(rv)) return rv;

  rv = NS_NOINTERFACE;
  
  if (aClass.Equals(kHTMLEditorCID)) {
    rv = GetEditorFactory(aFactory, aClass);
    if (NS_FAILED(rv)) goto done;
  }
  else if (aClass.Equals(kEditorShellCID)) {
    rv = GetEditorShellFactory(aFactory, aClass, aClassName, aProgID);
    if (NS_FAILED(rv)) goto done;  
  }

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, componentManager);

  return rv;
}

extern "C" NS_EXPORT PRBool
NSCanUnload(nsISupports* aServMgr)
{
  return nsEditor::gInstanceCount; 
}



extern "C" NS_EXPORT nsresult 
NSRegisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;
  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;


  rv = compMgr->RegisterComponent(kHTMLEditorCID, NULL, NULL, path, 
                                  PR_TRUE, PR_TRUE);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kEditorShellCID,
                                  "Editor Shell Component",
                                  "component://netscape/editor/editorshell",
                                  path, PR_TRUE, PR_TRUE);

  if (NS_FAILED(rv)) goto done;
  rv = compMgr->RegisterComponent(kEditorShellCID,
                                  "Editor Shell Spell Checker",
                                  "component://netscape/editor/editorspellcheck",
                                  path, PR_TRUE, PR_TRUE);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

extern "C" NS_EXPORT nsresult 
NSUnregisterSelf(nsISupports* aServMgr, const char *path)
{
  nsresult rv;

  nsCOMPtr<nsIServiceManager> servMgr(do_QueryInterface(aServMgr, &rv));
  if (NS_FAILED(rv)) return rv;

  nsIComponentManager* compMgr;
  rv = servMgr->GetService(kComponentManagerCID, 
                           nsIComponentManager::GetIID(), 
                           (nsISupports**)&compMgr);
  if (NS_FAILED(rv)) return rv;

/*
  rv = compMgr->UnregisterComponent(kEditorCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kTextEditorCID, path);
  if (NS_FAILED(rv)) goto done;
*/
  rv = compMgr->UnregisterComponent(kHTMLEditorCID, path);
  if (NS_FAILED(rv)) goto done;
  rv = compMgr->UnregisterComponent(kEditorShellCID, path);

  done:
  (void)servMgr->ReleaseService(kComponentManagerCID, compMgr);
  return rv;
}

//END EXPORTS


