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
 * IBM Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brian Ryner <bryner@brianryner.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIDOMUIListener_h__
#define nsIDOMUIListener_h__

#include "nsIDOMEventListener.h"

class nsIDOMEvent;

/*
 * UI event listener interface.
 */
// {5cb5527a-512f-4163-9393-ca95ceddbc13}
#define NS_IDOMUILISTENER_IID \
{ 0x5cb5527a, 0x512f, 0x4163, { 0x93, 0x93, 0xca, 0x95, 0xce, 0xdd, 0xbc, 0x13 } }

class nsIDOMUIListener : public nsIDOMEventListener {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IDOMUILISTENER_IID)

  NS_IMETHOD Activate(nsIDOMEvent* aEvent) = 0;
  NS_IMETHOD FocusIn(nsIDOMEvent* aEvent) = 0;
  NS_IMETHOD FocusOut(nsIDOMEvent* aEvent) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIDOMUIListener, NS_IDOMUILISTENER_IID)

#endif // nsIDOMUIListener_h__
