/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

#include "inDOMRDFResource.h"

inDOMRDFResource::inDOMRDFResource()
{
}

inDOMRDFResource::~inDOMRDFResource()
{
}

NS_IMPL_ISUPPORTS_INHERITED(inDOMRDFResource, nsRDFResource, inIDOMRDFResource)

NS_IMETHODIMP
inDOMRDFResource::SetObject(nsISupports* object)
{
  mObject = do_QueryInterface(object);
  return NS_OK;
}

NS_IMETHODIMP
inDOMRDFResource::GetObject(nsISupports** object)
{
  if (!object) return NS_ERROR_NULL_POINTER;

  *object = mObject;
  NS_IF_ADDREF(*object);
  
  return NS_OK;
}
  
NS_METHOD
inDOMRDFResource::Create(nsISupports* aOuter, const nsIID& iid, void **result) 
{
  inDOMRDFResource* ve = new inDOMRDFResource();
  if (!ve) return NS_ERROR_NULL_POINTER;
  NS_ADDREF(ve);
  nsresult rv = ve->QueryInterface(iid, result);
  NS_RELEASE(ve);
  return rv;
}

