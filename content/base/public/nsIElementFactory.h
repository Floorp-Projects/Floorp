/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIElementFactory_h___
#define nsIElementFactory_h___

#include "nsISupports.h"

class nsIContent;
class nsINodeInfo;

/* a6cf90fb-15b3-11d2-932e-00805f8add32 */
#define NS_IELEMENT_FACTORY_IID \
 { 0xa6cf90fb, 0x15b3, 0x11d2,{0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * An API for creating html content objects
 */
class nsIElementFactory : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IELEMENT_FACTORY_IID; return iid; }

  NS_IMETHOD CreateInstanceByTag(nsINodeInfo *aNodeInfo,
                                 nsIContent** aResult) = 0;
};

// ContractIDs for element factory registration
#define NS_ELEMENT_FACTORY_CONTRACTID          "@mozilla.org/layout/element-factory;1"
#define NS_ELEMENT_FACTORY_CONTRACTID_PREFIX   NS_ELEMENT_FACTORY_CONTRACTID "?namespace="

#define NS_HTML_NAMESPACE                  "http://www.w3.org/1999/xhtml"
#define NS_XML_NAMESPACE                   "http://www.w3.org/XML/1998/namespace"
 
#define NS_HTML_ELEMENT_FACTORY_CONTRACTID     NS_ELEMENT_FACTORY_CONTRACTID_PREFIX NS_HTML_NAMESPACE
#define NS_XML_ELEMENT_FACTORY_CONTRACTID      NS_ELEMENT_FACTORY_CONTRACTID_PREFIX NS_XML_NAMESPACE

#endif /* nsIElementFactory_h___ */
