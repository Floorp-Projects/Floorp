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

#ifndef nsLoadListenerProxy_h
#define nsLoadListenerProxy_h

#include "nsIDOMLoadListener.h"
#include "nsWeakReference.h"

/////////////////////////////////////////////
//
// This class exists to prevent a circular reference between
// the loaded document and the actual loader instance request. The
// request owns the document. While the document is loading, 
// the request is a load listener, held onto by the document.
// The proxy class breaks the circularity by filling in as the
// load listener and holding a weak reference to the request
// object.
//
/////////////////////////////////////////////

class nsLoadListenerProxy : public nsIDOMLoadListener {
public:
  nsLoadListenerProxy(nsWeakPtr aParent);
  virtual ~nsLoadListenerProxy();

  NS_DECL_ISUPPORTS

  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

  // nsIDOMLoadListener
  NS_IMETHOD Load(nsIDOMEvent* aEvent);
  NS_IMETHOD Unload(nsIDOMEvent* aEvent);
  NS_IMETHOD Abort(nsIDOMEvent* aEvent);
  NS_IMETHOD Error(nsIDOMEvent* aEvent);

protected:
  nsWeakPtr  mParent;
};

#endif
