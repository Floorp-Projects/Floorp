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

#ifndef nsDOMParser_h__
#define nsDOMParser_h__

#include "nsIDOMParser.h"
#include "nsISecurityCheckedComponent.h"
#include "nsISupportsUtils.h"
#include "nsCOMPtr.h"
#include "nsIURI.h"

#define IMPLEMENT_SYNC_LOAD
#ifdef IMPLEMENT_SYNC_LOAD
#include "nsIWebBrowserChrome.h"
#include "nsIDOMLoadListener.h"
#include "nsWeakReference.h"
#endif

class nsDOMParser : public nsIDOMParser
#ifdef IMPLEMENT_SYNC_LOAD
                    , public nsIDOMLoadListener
                    , public nsSupportsWeakReference
#endif
{
public: 
  nsDOMParser();
  virtual ~nsDOMParser();

  NS_DECL_ISUPPORTS

  // nsIDOMParser
  NS_DECL_NSIDOMPARSER

#ifdef IMPLEMENT_SYNC_LOAD
  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMLoadListener
  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);
#endif

private:
  nsCOMPtr<nsIURI> mBaseURI;

#ifdef IMPLEMENT_SYNC_LOAD
  nsCOMPtr<nsIWebBrowserChrome> mChromeWindow;
#endif
};

#endif
