/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsP3PUI_h_
#define nsP3PUI_h_

#include "nsIP3PUI.h"
#include "nsIP3PUIService.h"
#include "nsIP3PCService.h"
#include <nsIPref.h>
#include <nsCOMPtr.h>
#include <nsIDOMElement.h>
#include <nsIObserver.h>
#include <nsIDOMWindow.h>
#include <nsXPIDLString.h>
#include <nsString.h>
#include <nsIDocShell.h>
#include <nsIURI.h>
#include <nsIDocShellTreeItem.h>
#include <nsIWebProgressListener.h>
#include <nsIFormSubmitObserver.h>
#include <nsWeakReference.h>

#define P3P_STATUS_NO_P3P               0
#define P3P_STATUS_NO_POLICY            1
#define P3P_STATUS_POLICY_MET           2
#define P3P_STATUS_POLICY_NOT_MET       3
#define P3P_STATUS_PARTIAL_POLICY       4
#define P3P_STATUS_BAD_POLICY           5
#define P3P_STATUS_IN_PROGRESS          6

class nsP3PUI : public nsIP3PCUI,
                public nsIWebProgressListener,
                public nsSupportsWeakReference
{
public:

    nsP3PUI();
    virtual ~nsP3PUI();

    static NS_METHOD Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIP3PUI
    NS_DECL_NSIP3PCUI

protected:

    NS_METHOD DOMWindowToDocShellTreeItem( );

    nsCOMPtr<nsIP3PCService>            mP3PService;
    nsCOMPtr<nsIP3PUIService>           mP3PUIService;
    nsCOMPtr<nsIPref>                   mPrefServ;
    nsCOMPtr<nsIDOMWindow>              mWindow;
    nsCOMPtr<nsIDocShellTreeItem>       mDocShellTreeItem;
    nsCOMPtr<nsIDOMElement>             mPrivacyButton;
    nsCOMPtr<nsIURI>                    mCurrentURI;
    PRInt32                             mPrivacyStatus;
    PRBool                              mFirstInitialLoad;
};

#endif /* nsP3PUI_h_ */
