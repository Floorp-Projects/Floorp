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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef NS_POLICYREFERENCE_H__
#define NS_POLICYREFERENCE_H__

#include "nsCOMPtr.h"
#include "nsIURI.h"
#include "nsIXMLHttpRequest.h"
#include "nsIDOMEventListener.h"
#include "nsIDOMDocument.h"
#include "nsString.h"
#include "nsCRT.h"
#include "nsIPolicyReference.h"
#include "nsIPolicyListener.h"
#include "nsIPolicyTarget.h"
#include "nsWeakReference.h"

class nsPolicyReference : public nsIPolicyReference,
                          public nsIDOMEventListener,
                          public nsIPolicyTarget,
                          public nsSupportsWeakReference
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS
  
  nsPolicyReference();
  virtual ~nsPolicyReference( );
  
  // nsIPolicyReference
  NS_DECL_NSIPOLICYREFERENCE
  // nsIPolicyReference
  NS_DECL_NSIPOLICYTARGET
  // nsIDOMEventListener
  NS_IMETHOD HandleEvent(nsIDOMEvent  *aEvent);

protected:
  nsresult Load(const nsACString& aURI);
  nsresult ProcessPolicyReferenceFile(nsIDOMDocument* aDocument, char** aLocation);
  nsresult ProcessPolicyRefElement(nsIDOMDocument* aDocument, nsIDOMNodeList* aNodeList, nsAString& aPolicyLocation);
  nsresult ProcessPolicyRefChildren(nsIDOMNode* aNode);
  nsresult ProcessExpiryElement(nsIDOMNodeList* aNodeList);
  
  nsCOMPtr<nsIWeakReference>  mListener;
  nsCOMPtr<nsIXMLHttpRequest> mXMLHttpRequest;
  nsCOMPtr<nsIDOMDocument>    mDocument;
  nsCOMPtr<nsIURI>            mMainURI;
  nsCOMPtr<nsIURI>            mCurrentURI;
  nsCOMPtr<nsIURI>            mLinkedURI;
  PRUint32                    mFlags;
  PRUint32                    mError;
};

#endif
