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

#ifndef nsIP3PUI_h__
#define nsIP3PUI_h__

#include "nsISupports.h"
#include <nsIP3PUI.h>

#define NS_IP3PCUI_IID_STR "31430e62-d43d-11d3-9781-002035aee991"
#define NS_IP3PCUI_IID     {0x31430e62, 0xd43d, 0x11d3, { 0x97, 0x81, 0x00, 0x20, 0x35, 0xae, 0xe9, 0x91 }}

class nsIP3PCUI : public nsIP3PUI {
 public:

  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IP3PCUI_IID)

  NS_IMETHOD MarkNoP3P(void) = 0;

  NS_IMETHOD MarkNoPrivacy(void) = 0;

  NS_IMETHOD MarkPrivate(void) = 0;

  NS_IMETHOD MarkNotPrivate(void) = 0;

  NS_IMETHOD MarkPartialPrivacy(void) = 0;

  NS_IMETHOD MarkPrivacyBroken(void) = 0;

  NS_IMETHOD MarkInProgress(void) = 0;

};

/* Use this macro when declaring classes that implement this interface. */
#define NS_DECL_NSIP3PCUI               \
  NS_IMETHOD MarkNoP3P(void);           \
  NS_IMETHOD MarkNoPrivacy(void);       \
  NS_IMETHOD MarkPrivate(void);         \
  NS_IMETHOD MarkNotPrivate(void);      \
  NS_IMETHOD MarkPartialPrivacy(void);  \
  NS_IMETHOD MarkPrivacyBroken(void);   \
  NS_IMETHOD MarkInProgress(void);

#endif /* nsIP3PUI_h__ */
