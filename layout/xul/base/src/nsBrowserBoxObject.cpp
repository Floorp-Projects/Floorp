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
#include "nsIBrowserBoxObject.h"
#include "nsBoxObject.h"
#include "nsIPresShell.h"
#include "nsIFrame.h"
#include "nsIDocShell.h"
#include "nsXPIDLString.h"

class nsBrowserBoxObject : public nsIBrowserBoxObject, public nsBoxObject
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBROWSERBOXOBJECT

  nsBrowserBoxObject();
  virtual ~nsBrowserBoxObject();
  
protected:
  nsString* mSrcURL;
};

/* Implementation file */
NS_IMPL_ADDREF(nsBrowserBoxObject)
NS_IMPL_RELEASE(nsBrowserBoxObject)

NS_IMETHODIMP 
nsBrowserBoxObject::QueryInterface(REFNSIID iid, void** aResult)
{
  if (!aResult)
    return NS_ERROR_NULL_POINTER;
  
  if (iid.Equals(NS_GET_IID(nsIBrowserBoxObject))) {
    *aResult = (nsIBrowserBoxObject*)this;
    NS_ADDREF(this);
    return NS_OK;
  }

  return nsBoxObject::QueryInterface(iid, aResult);
}
  
nsBrowserBoxObject::nsBrowserBoxObject()
:mSrcURL(nsnull)
{
  NS_INIT_ISUPPORTS();
}

nsBrowserBoxObject::~nsBrowserBoxObject()
{
  /* destructor code */
  delete mSrcURL;
}

/* void openBrowser (in boolean openFlag); */
NS_IMETHODIMP nsBrowserBoxObject::GetDocShell(nsIDocShell** aResult)
{
  *aResult = nsnull;
  if (!mPresShell)
    return NS_OK;

  nsCOMPtr<nsISupports> subShell;
  mPresShell->GetSubShellFor(mContent, getter_AddRefs(subShell));
  if(!subShell)
    return NS_OK;

  return CallQueryInterface(subShell, aResult); //Addref happens here.
}

// Creation Routine ///////////////////////////////////////////////////////////////////////

nsresult
NS_NewBrowserBoxObject(nsIBoxObject** aResult)
{
  *aResult = new nsBrowserBoxObject;
  if (!*aResult)
    return NS_ERROR_OUT_OF_MEMORY;
  NS_ADDREF(*aResult);
  return NS_OK;
}

