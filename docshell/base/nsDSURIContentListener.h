/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsDSURIContentListener_h__
#define nsDSURIContentListener_h__

#include "nsCOMPtr.h"
#include "nsIURIContentListener.h"
#include "nsWeakReference.h"

class nsDocShell;
class nsIWebNavigationInfo;
class nsIHttpChannel;
class nsAString;

class nsDSURIContentListener :
    public nsIURIContentListener,
    public nsSupportsWeakReference

{
friend class nsDocShell;
public:
    NS_DECL_THREADSAFE_ISUPPORTS
    NS_DECL_NSIURICONTENTLISTENER

    nsresult Init();

protected:
    explicit nsDSURIContentListener(nsDocShell* aDocShell);
    virtual ~nsDSURIContentListener();

    void DropDocShellreference() {
        mDocShell = nullptr;
    }

    // Determine if X-Frame-Options allows content to be framed
    // as a subdocument
    bool CheckFrameOptions(nsIRequest* request);
    bool CheckOneFrameOptionsPolicy(nsIHttpChannel* httpChannel,
                                    const nsAString& policy);

    enum XFOHeader {
      eDENY,
      eSAMEORIGIN,
      eALLOWFROM
    };

    void ReportXFOViolation(nsIDocShellTreeItem* aTopDocShellItem,
                            nsIURI* aThisURI,
                            XFOHeader aHeader);
protected:
    nsDocShell*                      mDocShell;

    // Store the parent listener in either of these depending on
    // if supports weak references or not. Proper weak refs are
    // preferred and encouraged!
    nsWeakPtr                        mWeakParentContentListener;
    nsIURIContentListener*           mParentContentListener;

    nsCOMPtr<nsIWebNavigationInfo>   mNavInfo;
};

#endif /* nsDSURIContentListener_h__ */
