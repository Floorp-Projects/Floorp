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
#ifndef nsIXMLElementFactory_h___
#define nsIXMLElementFactory_h___

#include "nslayout.h"
#include "nsISupports.h"

class nsIXMLContent;
class nsString;

// {CF170390-79CC-11d3-BE44-0020A6361667}
#define NS_IXML_ELEMENT_FACTORY_IID \
{ 0xcf170390, 0x79cc, 0x11d3, { 0xbe, 0x44, 0x0, 0x20, 0xa6, 0x36, 0x16, 0x67 } };

/**
 * An API for creating XML content objects
 */
class nsIXMLElementFactory : public nsISupports {
public:
  static const nsIID& GetIID() { static nsIID iid = NS_IXML_ELEMENT_FACTORY_IID; return iid; }

  NS_IMETHOD CreateInstanceByTag(const nsString& aTag,
                                 nsIXMLContent** aResult) = 0;
};


extern nsresult
NS_NewXMLElementFactory(nsIXMLElementFactory** aResult);

#endif /* nsIXMLElementFactory_h___ */
