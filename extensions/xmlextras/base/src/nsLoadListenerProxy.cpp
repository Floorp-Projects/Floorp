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

#include "nsLoadListenerProxy.h"
#include "nsIDOMEvent.h"

nsLoadListenerProxy::nsLoadListenerProxy(nsWeakPtr aParent)
{
  NS_INIT_ISUPPORTS();
  mParent = aParent;
}

nsLoadListenerProxy::~nsLoadListenerProxy()
{
}

NS_IMPL_ISUPPORTS1(nsLoadListenerProxy, nsIDOMLoadListener)

NS_IMETHODIMP
nsLoadListenerProxy::HandleEvent(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->HandleEvent(aEvent);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Load(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Load(aEvent);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Unload(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Unload(aEvent);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Abort(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Abort(aEvent);
  }
  
  return NS_OK;
}

NS_IMETHODIMP
nsLoadListenerProxy::Error(nsIDOMEvent* aEvent)
{
  nsCOMPtr<nsIDOMLoadListener> listener(do_QueryReferent(mParent));

  if (listener) {
    return listener->Error(aEvent);
  }
  
  return NS_OK;
}
