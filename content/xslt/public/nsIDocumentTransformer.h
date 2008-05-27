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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Jonas Sicking <sicking@bigfoot.com> (Original author)
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
 
#ifndef nsIDocumentTransformer_h__
#define nsIDocumentTransformer_h__

#include "nsISupports.h"

class nsIDOMDocument;
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
  {0x17c83d91, 0xac2f, 0x4658, \
    { 0x91, 0x6c, 0xcb, 0xc4, 0xd2, 0xb5, 0x2c, 0xe }}

class nsIDocumentTransformer : public nsISupports
{
public: 

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOCUMENTTRANSFORMER_IID)

  NS_IMETHOD Init(nsIPrincipal* aPrincipal) = 0;
  NS_IMETHOD SetTransformObserver(nsITransformObserver* aObserver) = 0;
  NS_IMETHOD LoadStyleSheet(nsIURI* aUri, nsILoadGroup* aLoadGroup) = 0;
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
