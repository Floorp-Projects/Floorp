/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *         Jonas Sicking <jonas@sicking.cc> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/*
 * Content policy implementation that prevents all loads of images,
 * subframes, etc from protocols that don't return data but rather open
 * applications (such as mailto).
 */

#include "nsNoDataProtocolContentPolicy.h"
#include "nsIDocument.h"
#include "nsINode.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"
#include "nsString.h"
#include "nsContentUtils.h"
#include "nsIProtocolHandler.h"
#include "nsIIOService.h"
#include "nsIExternalProtocolHandler.h"

NS_IMPL_ISUPPORTS1(nsNoDataProtocolContentPolicy, nsIContentPolicy)

NS_IMETHODIMP
nsNoDataProtocolContentPolicy::ShouldLoad(PRUint32 aContentType,
                                          nsIURI *aContentLocation,
                                          nsIURI *aRequestingLocation,
                                          nsISupports *aRequestingContext,
                                          const nsACString &aMimeGuess,
                                          nsISupports *aExtra,
                                          PRInt16 *aDecision)
{
  *aDecision = nsIContentPolicy::ACCEPT;

  if (aContentType == TYPE_OTHER ||
      aContentType == TYPE_SCRIPT ||
      aContentType == TYPE_IMAGE ||
      aContentType == TYPE_STYLESHEET ||
      aContentType == TYPE_OBJECT) {
    nsCAutoString scheme;
    aContentLocation->GetScheme(scheme);
    // Fast-track for the common cases
    if (scheme.EqualsLiteral("http") ||
        scheme.EqualsLiteral("https") ||
        scheme.EqualsLiteral("ftp") ||
        scheme.EqualsLiteral("file") ||
        scheme.EqualsLiteral("chrome")) {
      return NS_OK;
    }

    nsIIOService* ios = nsContentUtils::GetIOService();
    if (!ios) {
      // default to accept, just in case
      return NS_OK;
    }
    
    nsCOMPtr<nsIProtocolHandler> handler;
    ios->GetProtocolHandler(scheme.get(), getter_AddRefs(handler));

    nsCOMPtr<nsIExternalProtocolHandler> extHandler =
      do_QueryInterface(handler);

    if (extHandler) {
      *aDecision = nsIContentPolicy::REJECT_SERVER;
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsNoDataProtocolContentPolicy::ShouldProcess(PRUint32 aContentType,
                                             nsIURI *aContentLocation,
                                             nsIURI *aRequestingLocation,
                                             nsISupports *aRequestingContext,
                                             const nsACString &aMimeGuess,
                                             nsISupports *aExtra,
                                             PRInt16 *aDecision)
{
  return ShouldLoad(aContentType, aContentLocation, aRequestingLocation,
                    aRequestingContext, aMimeGuess, aExtra, aDecision);
}
