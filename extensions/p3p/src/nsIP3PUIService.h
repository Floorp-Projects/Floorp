/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and imitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is International
 * Business Machines Corporation. Portions created by IBM
 * Corporation are Copyright (C) 2000 International Business
 * Machines Corporation. All Rights Reserved.
 *
 * Contributor(s): IBM Corporation.
 *
 */

#ifndef nsIP3PUIService_h__
#define nsIP3PUIService_h__

#include "nsISupports.h"
#include "nsIP3PCUI.h"
#include <nsIDocShellTreeItem.h>
#include <nsIDOMWindowInternal.h>


#define NS_IP3PUISERVICE_IID_STR "31430e52-d43d-11d3-9781-002035aee991"
#define NS_IP3PUISERVICE_IID {0x31430e52, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

#define NS_P3PUISERVICE_CONTRACTID "@mozilla.org/p3p-ui-service;1"
#define NS_P3PUISERVICE_CLASSNAME "P3P UI Service"
#define NS_P3PUISERVICE_CID {0x31430e53, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

class nsIP3PUIService : public nsISupports {
 public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PUISERVICE_IID)

  NS_IMETHOD RegisterUIInstance( nsIDocShellTreeItem * aDocShellTreeItem,
                                 nsIP3PCUI * P3PCUI ) = 0;

  NS_IMETHOD DeregisterUIInstance( nsIDocShellTreeItem * aDocShellTreeItem,
                                   nsIP3PCUI * P3PCUI ) = 0;

  NS_IMETHOD MarkNoP3P( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD MarkNoPrivacy( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD MarkPrivate( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD MarkNotPrivate( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD MarkPartialPrivacy( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD MarkPrivacyBroken( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD MarkInProgress( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD WarningNotPrivate( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD WarningPartialPrivacy( nsIDocShellTreeItem * aDocShellTreeItem ) = 0;

  NS_IMETHOD WarningPostToNotPrivate( nsIDOMWindowInternal * aDOMWindowInternal,
                                      PRBool * aRetval ) = 0;

  NS_IMETHOD WarningPostToBrokenPolicy( nsIDOMWindowInternal * aDOMWindowInternal,
                                        PRBool * aRetval ) = 0;

  NS_IMETHOD WarningPostToNoPolicy( nsIDOMWindowInternal * aDOMWindowInternal,
                                    PRBool * aRetval ) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIP3PUISERVICE                                                    \
  NS_IMETHOD RegisterUIInstance( nsIDocShellTreeItem * aDocShellTreeItem,          \
                                 nsIP3PCUI * P3PCUI );                             \
  NS_IMETHOD DeregisterUIInstance( nsIDocShellTreeItem * aDocShellTreeItem,        \
                                   nsIP3PCUI * P3PCUI );                           \
  NS_IMETHOD MarkNoP3P( nsIDocShellTreeItem * aDocShellTreeItem );                 \
  NS_IMETHOD MarkNoPrivacy( nsIDocShellTreeItem * aDocShellTreeItem );             \
  NS_IMETHOD MarkPrivate( nsIDocShellTreeItem * aDocShellTreeItem );               \
  NS_IMETHOD MarkNotPrivate( nsIDocShellTreeItem * aDocShellTreeItem );            \
  NS_IMETHOD MarkPartialPrivacy( nsIDocShellTreeItem * aDocShellTreeItem );        \
  NS_IMETHOD MarkPrivacyBroken( nsIDocShellTreeItem * aDocShellTreeItem );         \
  NS_IMETHOD MarkInProgress( nsIDocShellTreeItem * aDocShellTreeItem );            \
  NS_IMETHOD WarningNotPrivate( nsIDocShellTreeItem * aDocShellTreeItem );         \
  NS_IMETHOD WarningPartialPrivacy( nsIDocShellTreeItem * aDocShellTreeItem );     \
  NS_IMETHOD WarningPostToNotPrivate( nsIDOMWindowInternal * aDOMWindowInternal,   \
                                      PRBool * aRetval );                          \
  NS_IMETHOD WarningPostToBrokenPolicy( nsIDOMWindowInternal * aDOMWindowInternal, \
                                        PRBool * aRetval );                        \
  NS_IMETHOD WarningPostToNoPolicy( nsIDOMWindowInternal * aDOMWindowInternal,     \
                                    PRBool * aRetval );

#endif /* nsIP3PUIService_h__ */
