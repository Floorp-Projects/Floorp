/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#ifndef nsSyncLoader_h__
#define nsSyncLoader_h__

#include "nsCOMPtr.h"
#include "nsIChannel.h"
#include "nsIDOMLoadListener.h"
#include "nsISyncLoader.h"
#include "nsString.h"
#include "nsWeakReference.h"

class nsSyncLoader : public nsISyncLoader,
                     public nsIDOMLoadListener,
                     public nsSupportsWeakReference
{
public:
    nsSyncLoader();
    virtual ~nsSyncLoader();

    NS_DECL_ISUPPORTS

    NS_DECL_NSISYNCLOADER

    // nsIDOMEventListener
    NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);

    // nsIDOMLoadListener
    NS_IMETHOD Load(nsIDOMEvent* aEvent);
    NS_IMETHOD Unload(nsIDOMEvent* aEvent);
    NS_IMETHOD Abort(nsIDOMEvent* aEvent);
    NS_IMETHOD Error(nsIDOMEvent* aEvent);

protected:
    nsCOMPtr<nsIChannel> mChannel;
    PRBool mLoading, mLoadSuccess;
};

#endif
