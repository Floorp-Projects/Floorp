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
  NS_IMETHOD SetDocument(nsIDocument* aDocument);
  
protected:
  nsCOMPtr<nsIEditorShell> mEditorShell;
};

/* Implementation file */
NS_IMPL_ADDREF(nsEditorBoxObject)
NS_IMPL_RELEASE(nsEditorBoxObject)


NS_IMETHODIMP
nsEditorBoxObject::SetDocument(nsIDocument* aDocument)
{
  mEditorShell = nsnull;
  return nsBoxObject::SetDocument(aDocument);
}


NS_IMETHODIMP 
nsEditorBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIEditorBoxObject))) {
    *aResult = (nsIEditorBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsEditorBoxObject::nsEditorBoxObject()
{
  NS_INIT_ISUPPORTS();
}

nsEditorBoxObject::~nsEditorBoxObject()
{
  /* destructor code */
}

/* void openEditor (in boolean openFlag); */
NS_IMETHODIMP nsEditorBoxObject::GetEditorShell(nsIEditorShell** aResult)
{
  if (!mEditorShell) {
    mEditorShell = do_CreateInstance("@mozilla.org/editor/editorshell;1");
  }

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

