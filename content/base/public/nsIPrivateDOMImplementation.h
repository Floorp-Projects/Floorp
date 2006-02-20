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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

#ifndef nsIPrivateDOMImplementation_h__
#define nsIPrivateDOMImplementation_h__

#include "nsISupports.h"

class nsIURI;
class nsIPrincipal;

/*
 * Event listener manager interface.
 */
#define NS_IPRIVATEDOMIMPLEMENTATION_IID \
{ /* 87c20441-8b0d-4383-a189-52fef1dd5d8a */ \
0x87c20441, 0x8b0d, 0x4383, \
 { 0xa1, 0x89, 0x52, 0xfe, 0xf1, 0xdd, 0x5d, 0x8a } }

class nsIPrivateDOMImplementation : public nsISupports {

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATEDOMIMPLEMENTATION_IID)

  NS_IMETHOD Init(nsIURI* aDocumentURI, nsIURI* aBaseURI,
                  nsIPrincipal* aPrincipal) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateDOMImplementation,
                              NS_IPRIVATEDOMIMPLEMENTATION_IID)

nsresult
NS_NewDOMImplementation(nsIDOMDOMImplementation** aInstancePtrResult);

#endif // nsIPrivateDOMImplementation_h__
