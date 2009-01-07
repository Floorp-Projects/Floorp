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

#ifndef nsIPrivateTextEvent_h__
#define nsIPrivateTextEvent_h__

#include "nsEvent.h"
#include "nsISupports.h"
#include "nsIPrivateTextRange.h"

#define NS_IPRIVATETEXTEVENT_IID \
{0xeee4a0f0, 0x7fb0, 0x494c, \
{0x80, 0xe3, 0x30, 0xd3, 0x08, 0x95, 0x15, 0x2c}}

class nsIPrivateTextEvent : public nsISupports {

public:
	NS_DECLARE_STATIC_IID_ACCESSOR(NS_IPRIVATETEXTEVENT_IID)

	NS_IMETHOD GetText(nsString& aText) = 0;
	NS_IMETHOD_(already_AddRefed<nsIPrivateTextRangeList>) GetInputRange() = 0;
	NS_IMETHOD_(nsTextEventReply*) GetEventReply() = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIPrivateTextEvent, NS_IPRIVATETEXTEVENT_IID)

#endif // nsIPrivateTextEvent_h__

