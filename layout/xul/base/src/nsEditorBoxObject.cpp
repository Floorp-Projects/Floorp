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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIEditorBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIEditorShell.h"

class nsEditorBoxObject : public nsIEditorBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIEDITORBOXOBJECT

  nsEditorBoxObject();
  virtual ~nsEditorBoxObject();
//NS_PIBOXOBJECT interfaces
  NS_IMETHOD Init(nsIContent* aContent, nsIPresShell* aPresShell);
  NS_IMETHOD SetDocument(nsIDocument* aDocument);
  
protected:
  nsCOMPtr<nsIEditorShell> mEditorShell;
};

/* Implementation file */
NS_IMPL_ISUPPORTS_INHERITED(nsEditorBoxObject, nsBoxObject, nsIEditorBoxObject)


NS_IMETHODIMP
nsEditorBoxObject::SetDocument(nsIDocument* aDocument)
{
  nsresult rv;
  
  if (mEditorShell)
  {
    rv = mEditorShell->Shutdown();
    NS_ASSERTION(NS_SUCCEEDED(rv), "Error from editorShell->Shutdown");
  }
  
  // this should only be called with a null document, which indicates
  // that we're being torn down.
  NS_ASSERTION(aDocument == nsnull, "SetDocument called with non-null document");
  mEditorShell = nsnull;
  return nsBoxObject::SetDocument(aDocument);
}

  
nsEditorBoxObject::nsEditorBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsEditorBoxObject::~nsEditorBoxObject()
{
  /* destructor code */
}


NS_IMETHODIMP nsEditorBoxObject::Init(nsIContent* aContent, nsIPresShell* aPresShell)
{
  nsresult rv = nsBoxObject::Init(aContent, aPresShell);
  if (NS_FAILED(rv)) return rv;
  
  NS_ASSERTION(!mEditorShell, "Double init of nsEditorBoxObject");
  
  mEditorShell = do_CreateInstance("@mozilla.org/editor/editorshell;1");
  if (!mEditorShell) return NS_ERROR_OUT_OF_MEMORY;

  rv = mEditorShell->Init();
  if (NS_FAILED(rv)) return rv;
  
  return NS_OK;
}


NS_IMETHODIMP nsEditorBoxObject::GetEditorShell(nsIEditorShell** aResult)
{
  NS_ASSERTION(mEditorShell, "Editor box object not initted");

  *aResult = mEditorShell;
  NS_IF_ADDREF(*aResult);
  return NS_OK;
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewEditorBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsEditorBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

