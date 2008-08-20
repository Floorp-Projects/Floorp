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
 * The Original Code is mozilla.org code.
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

#ifndef nsIPrivateCompositionEvent_h__
#define nsIPrivateCompositionEvent_h__

#include "nsEvent.h"
#include "nsISupports.h"

// {901B82D5-67C0-45ad-86AE-AB9A6BD74111}
#define NS_IPRIVATECOMPOSITIONEVENT_IID	\
{ 0x901b82d5, 0x67c0, 0x45ad, \
{ 0x86, 0xae, 0xab, 0x9a, 0x6b, 0xd7, 0x41, 0x11 } }

class nsIPrivateCompositionEvent : public nsISupports {

public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATECOMPOSITIONEVENT_IID)

  NS_IMETHOD GetCompositionReply(struct nsTextEventReply** aReply) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateCompositionEvent,
                              NS_IPRIVATECOMPOSITIONEVENT_IID)

#endif // nsIPrivateCompositionEvent_h__

