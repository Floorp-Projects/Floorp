/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
// vim:cindent:ts=8:et:sw=4:
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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   L. David Baron <dbaron@fas.harvard.edu> (original author)
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

#ifndef nsICSSOMFactory_h___
#define nsICSSOMFactory_h___

#include "nsISupports.h"
class nsDOMCSSDeclaration;
class nsIHTMLContent;

// bfdd87bd-79c8-4041-ae14-91fa8536ce61
#define NS_ICSSOMFACTORY_IID \
  { 0xbfdd87bd, 0x79c8, 0x4041, \
    {0xae, 0x14, 0x91, 0xfa, 0x85, 0x36, 0xce, 0x61 } }

// 5fcaa2c1-7ca4-4f73-a357-93e79d709376
#define NS_CSSOMFACTORY_CID \
  { 0x5fcaa2c1, 0x7ca4, 0x4f73, \
    {0xa3, 0x57, 0x93, 0xe7, 0x9d, 0x70, 0x93, 0x76 } }

class nsICSSOMFactory : public nsISupports {
public:
    NS_DEFINE_STATIC_IID_ACCESSOR(NS_ICSSOMFACTORY_IID)

    NS_IMETHOD CreateDOMCSSAttributeDeclaration(nsIHTMLContent *aContent,
                                                nsDOMCSSDeclaration **aResult) = 0;
};

#endif /* nsICSSOMFactory_h___ */
