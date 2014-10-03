/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
 
#ifndef nsIDocumentTransformer_h__
#define nsIDocumentTransformer_h__

#include "nsISupports.h"

class nsIDOMNode;
class nsILoadGroup;
class nsIURI;
class nsIPrincipal;
class nsString;

#define NS_ITRANSFORMOBSERVER_IID \
{ 0x04b2d17c, 0xe98d, 0x45f5, \
  { 0x9a, 0x67, 0xb7, 0x01, 0x19, 0x59, 0x7d, 0xe7 } }

class nsITransformObserver : public nsISupports
{
public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITRANSFORMOBSERVER_IID)

  NS_IMETHOD OnDocumentCreated(nsIDocument *aResultDocument) = 0;

  NS_IMETHOD OnTransformDone(nsresult aResult,
                             nsIDocument *aResultDocument) = 0;

};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITransformObserver, NS_ITRANSFORMOBSERVER_IID)

#define NS_IDOCUMENTTRANSFORMER_IID \
{ 0xc75613ee, 0xcb6e, 0x4bf7, \
 { 0x9e, 0x5b, 0x36, 0x50, 0x69, 0xc1, 0xe3, 0x55 } }

class nsIDocumentTransformer : public nsISupports
{
public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENTTRANSFORMER_IID)

  NS_IMETHOD SetTransformObserver(nsITransformObserver* aObserver) = 0;
  NS_IMETHOD LoadStyleSheet(nsIURI* aUri, nsILoadGroup* aLoadGroup,
                            nsIPrincipal* aPrincipal) = 0;
  NS_IMETHOD SetSourceContentModel(nsIDOMNode* aSource) = 0;
  NS_IMETHOD CancelLoads() = 0;

  NS_IMETHOD AddXSLTParamNamespace(const nsString& aPrefix,
                                   const nsString& aNamespace) = 0;
  NS_IMETHOD AddXSLTParam(const nsString& aName,
                          const nsString& aNamespace,
                          const nsString& aValue,
                          const nsString& aSelect,
                          nsIDOMNode* aContextNode) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDocumentTransformer,
                              NS_IDOCUMENTTRANSFORMER_IID)

#endif //nsIDocumentTransformer_h__
