/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * Original Author: Scott MacGregor (mscott@netscape.com)
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
