/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * Original Author: Scott MacGregor (mscott@netscape.com)
 *
 * Contributor(s): 
 */

/*

  Private interface to the XBL PrototypeProperty

*/

#ifndef nsIXBLPrototypeProperty_h__
#define nsIXBLPrototypeProperty_h__

class nsIContent;
class nsIScriptContext;
class nsIXBLPrototypeBinding;

// {FDDD1C5C-F47C-4b10-9CBC-34E087D1279A}
#define NS_IXBLPROTOTYPEPROPERTY_IID \
{ 0xfddd1c5c, 0xf47c, 0x4b10, { 0x9c, 0xbc, 0x34, 0xe0, 0x87, 0xd1, 0x27, 0x9a } }

class nsIXBLPrototypeProperty : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IXBLPROTOTYPEPROPERTY_IID)

  NS_IMETHOD GetNextProperty(nsIXBLPrototypeProperty** aProperty) = 0;
  NS_IMETHOD SetNextProperty(nsIXBLPrototypeProperty* aProperty) = 0;

  NS_IMETHOD ConstructProperty(nsIContent * aInterfaceElement, nsIContent* aPropertyElement) = 0;
  NS_IMETHOD InstallProperty(nsIScriptContext * aContext, nsIContent *aBoundElement, void * aScriptObject, void * aTargetClassObject) = 0;
  NS_IMETHOD InitTargetObjects(nsIScriptContext * aContext, nsIContent * aBoundElement, void ** aScriptObject, void ** aTargetClassObject) = 0;
};

extern nsresult
NS_NewXBLPrototypeProperty(nsIXBLPrototypeBinding * aPrototypeBinding, nsIXBLPrototypeProperty ** aResult);

#endif // nsIXBLPrototypeProperty_h__
