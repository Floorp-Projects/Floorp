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

#define NS_ITRANSFORMOBSERVER_IID \
  {0xcce88481, 0x6eb3, 0x11d6, \
    { 0xa7, 0xf2, 0x8d, 0x82, 0xcd, 0x2a, 0xf3, 0x7c }}

class nsITransformObserver : public nsISupports
{
public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_ITRANSFORMOBSERVER_IID)

  NS_IMETHOD OnDocumentCreated(nsIDOMDocument *aResultDocument) = 0;

  NS_IMETHOD OnTransformDone(nsresult aResult,
                             nsIDOMDocument *aResultDocument) = 0;

};

#define NS_IDOCUMENTTRANSFORMER_IID \
  {0x43e5a6c6, 0xa53c, 0x4f97, \
    { 0x91, 0x79, 0x47, 0xf2, 0x46, 0xec, 0xd9, 0xd6 }}

class nsIDocumentTransformer : public nsISupports
{
public: 

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IDOCUMENTTRANSFORMER_IID)

  NS_IMETHOD SetTransformObserver(nsITransformObserver* aObserver) = 0;
  NS_IMETHOD LoadStyleSheet(nsIURI* aUri, nsILoadGroup* aLoadGroup,
                            nsIPrincipal* aCallerPrincipal) = 0;
  NS_IMETHOD SetSourceContentModel(nsIDOMNode* aSource) = 0;
  NS_IMETHOD CancelLoads() = 0;
};

#endif //nsIDocumentTransformer_h__
