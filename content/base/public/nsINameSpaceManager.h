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
 * The Original Code is Mozilla Communicator client code.
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

#ifndef nsINameSpaceManager_h___
#define nsINameSpaceManager_h___

#include "nsISupports.h"
#include "nsStringGlue.h"

class nsIAtom;
class nsString;

#define kNameSpaceID_Unknown -1
// 0 is special at C++, so use a static const PRInt32 for
// kNameSpaceID_None to keep if from being cast to pointers
static const PRInt32 kNameSpaceID_None = 0;
#define kNameSpaceID_XMLNS    1 // not really a namespace, but it needs to play the game
#define kNameSpaceID_XML      2
#define kNameSpaceID_XHTML    3
#define kNameSpaceID_XLink    4
#define kNameSpaceID_XSLT     5
#define kNameSpaceID_XBL      6
#define kNameSpaceID_MathML   7
#define kNameSpaceID_RDF      8
#define kNameSpaceID_XUL      9
#define kNameSpaceID_SVG      10
#define kNameSpaceID_XMLEvents 11
#define kNameSpaceID_LastBuiltin          11 // last 'built-in' namespace

#define NS_NAMESPACEMANAGER_CONTRACTID "@mozilla.org/content/namespacemanager;1"

#define NS_INAMESPACEMANAGER_IID \
  { 0xd74e83e6, 0xf932, 0x4289, \
    { 0xac, 0x95, 0x9e, 0x10, 0x24, 0x30, 0x88, 0xd6 } }

/**
 * The Name Space Manager tracks the associtation between a NameSpace
 * URI and the PRInt32 runtime id. Mappings between NameSpaces and 
 * NameSpace prefixes are managed by nsINameSpaces.
 *
 * All NameSpace URIs are stored in a global table so that IDs are
 * consistent accross the app. NameSpace IDs are only consistent at runtime
 * ie: they are not guaranteed to be consistent accross app sessions.
 *
 * The nsINameSpaceManager needs to have a live reference for as long as
 * the NameSpace IDs are needed.
 *
 */

class nsINameSpaceManager : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_INAMESPACEMANAGER_IID)

  virtual nsresult RegisterNameSpace(const nsAString& aURI,
                                     PRInt32& aNameSpaceID) = 0;

  virtual nsresult GetNameSpaceURI(PRInt32 aNameSpaceID, nsAString& aURI) = 0;
  virtual PRInt32 GetNameSpaceID(const nsAString& aURI) = 0;

  virtual PRBool HasElementCreator(PRInt32 aNameSpaceID) = 0;
};
 
NS_DEFINE_STATIC_IID_ACCESSOR(nsINameSpaceManager, NS_INAMESPACEMANAGER_IID)

nsresult NS_GetNameSpaceManager(nsINameSpaceManager** aInstancePtrResult);

void NS_NameSpaceManagerShutdown();

#endif // nsINameSpaceManager_h___
