/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is Zero-Knowledge Systems,
 * Inc.  Portions created by Zero-Knowledge are Copyright (C) 2000
 * Zero-Knowledge Systems, Inc.  All Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsIContentPolicy.h"
#include "nsISupportsArray.h"

#ifndef __nsContentPolicy_h__
#define __nsContentPolicy_h__

class nsContentPolicy : public nsIContentPolicy
{
 public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSICONTENTPOLICY
    nsContentPolicy();
    virtual ~nsContentPolicy();
 private:
    nsCOMPtr<nsISupportsArray> mPolicies;
    NS_IMETHOD CheckPolicy(PRInt32 policyType, PRInt32 contentType,
                           nsIURI *aURI, nsISupports *context,
                           nsIDOMWindow *window, PRBool *shouldProceed);
};

nsresult
NS_NewContentPolicy(nsIContentPolicy **aResult);

#endif /* __nsContentPolicy_h__ */
