/* -*- Mode: c++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
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

// ProgIDs for element factory registration
#define NS_ELEMENT_FACTORY_PROGID          "component://netscape/layout/element-factory"
#define NS_ELEMENT_FACTORY_PROGID_PREFIX   NS_ELEMENT_FACTORY_PROGID "?namespace="

#define NS_HTML_NAMESPACE                  "http://www.w3.org/TR/REC-html40"
#define NS_XML_NAMESPACE                   "http://www.w3.org/XML/1998/namespace"
 
#define NS_HTML_ELEMENT_FACTORY_PROGID     NS_ELEMENT_FACTORY_PROGID_PREFIX NS_HTML_NAMESPACE
#define NS_XML_ELEMENT_FACTORY_PROGID      NS_ELEMENT_FACTORY_PROGID_PREFIX NS_XML_NAMESPACE

#endif /* nsIElementFactory_h___ */
