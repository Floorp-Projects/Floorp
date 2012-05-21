/* -*- Mode: IDL; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef nsIXPathEvaluatorInternal_h__
#define nsIXPathEvaluatorInternal_h__

#include "nsCOMArray.h"
#include "nsTArray.h"
#include "nsStringGlue.h"

class nsIDOMDocument;
class nsIDOMXPathExpression;
class nsIDOMXPathNSResolver;

#define NS_IXPATHEVALUATORINTERNAL_IID \
  {0xb4b72daa, 0x65d6, 0x440f, \
    { 0xb6, 0x08, 0xe2, 0xee, 0x9a, 0x82, 0xf3, 0x13 }}

class nsIXPathEvaluatorInternal : public nsISupports
{
public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IXPATHEVALUATORINTERNAL_IID)

  /**
   * Sets the document this evaluator corresponds to
   */
  NS_IMETHOD SetDocument(nsIDOMDocument* aDocument) = 0;

  NS_IMETHOD CreateExpression(const nsAString &aExpression,
                              nsIDOMXPathNSResolver *aResolver,
                              nsTArray<nsString> *aNamespaceURIs,
                              nsTArray<nsCString> *aContractIDs,
                              nsCOMArray<nsISupports> *aState,
                              nsIDOMXPathExpression **aResult) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIXPathEvaluatorInternal,
                              NS_IXPATHEVALUATORINTERNAL_IID)

#endif //nsIXPathEvaluatorInternal_h__
