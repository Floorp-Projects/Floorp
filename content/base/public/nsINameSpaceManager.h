/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef nsINameSpaceManager_h___
#define nsINameSpaceManager_h___

#include "nsISupports.h"
#include "nslayout.h"
#include "nsAWritableString.h"

class nsIAtom;
class nsString;
class nsINameSpace;
class nsIElementFactory;

#define kNameSpaceID_Unknown -1
#define kNameSpaceID_None     0
#define kNameSpaceID_XMLNS    1 // not really a namespace, but it needs to play the game
#define kNameSpaceID_XML      2
#define kNameSpaceID_HTML     3
#define kNameSpaceID_XLink    4
#define kNameSpaceID_HTML2    5 // This is not a real namespace
#define kNameSpaceID_XSLT     6
#define kNameSpaceID_XHTML    kNameSpaceID_HTML

// 'html' is by definition bound to the namespace name "urn:w3-org-ns:HTML" XXX ???
// 'xml' is by definition bound to the namespace name "urn:Connolly:input:required" XXX

#define NS_INAMESPACEMANAGER_IID \
  { 0xa6cf90d5, 0x15b3, 0x11d2, \
    {0x93, 0x2e, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32}}

/**
 * The Name Space Manager tracks the associtation between a NameSpace
 * URI and the PRInt32 runtime id. Mappings between NameSpaces and 
 * NameSpace prefixes are managed by nsINameSpaces
 *
 * All NameSpace URIs are stored in a global table so that IDs are
 * consistent accross the app. NameSpace IDs are only consistent at runtime
 * ie: they are not guaranteed to be consistent accross app sessions.
 *
 * The nsINameSpaceManager needs to have a live reference for as long as
 * the NameSpace IDs are needed. Generally, a document keeps a reference to 
 * a nsINameSpaceManager. Also, each nsINameSpace that comes from the manager
 * keeps a reference to it.
 *
 * To create a stack of NameSpaces, call CreateRootNameSpace, and then create
 * child NameSpaces from the root.
 *
 * The "html" and "xml" namespaces come "pre-canned" from the root.
 *
 */

class nsINameSpaceManager : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_INAMESPACEMANAGER_IID)

  NS_IMETHOD CreateRootNameSpace(nsINameSpace*& aRootNameSpace) = 0;

  NS_IMETHOD RegisterNameSpace(const nsAReadableString& aURI, 
			                         PRInt32& aNameSpaceID) = 0;

  NS_IMETHOD GetNameSpaceURI(PRInt32 aNameSpaceID,
                             nsAWritableString& aURI) = 0;
  NS_IMETHOD GetNameSpaceID(const nsAReadableString& aURI,
                            PRInt32& aNameSpaceID) = 0;

  NS_IMETHOD GetElementFactory(PRInt32 aNameSpaceID,
                               nsIElementFactory **aElementFactory) = 0;
};

nsresult
  NS_NewNameSpaceManager(nsINameSpaceManager** aInstancePtrResult);

void
  NS_NameSpaceManagerShutdown();


#endif // nsINameSpaceManager_h___
