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
 * Peter Van der Beken, peterv@netscape.com
 *    -- original author.
 *
 */

#ifndef nsSyncLoader_h__
#define nsSyncLoader_h__

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIDOMLoadListener.h"
#include "nsIDocument.h"
#include "nsIDocShellTreeOwner.h"
#include "nsWeakReference.h"
#include "nsISyncLoader.h"

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
    virtual nsresult HandleEvent(nsIDOMEvent* aEvent);

    // nsIDOMLoadListener
    virtual nsresult Load(nsIDOMEvent* aEvent);
    virtual nsresult Unload(nsIDOMEvent* aEvent);
    virtual nsresult Abort(nsIDOMEvent* aEvent);
    virtual nsresult Error(nsIDOMEvent* aEvent);

protected:
    nsCOMPtr<nsIDocShellTreeOwner> mDocShellTreeOwner;
};

#endif
