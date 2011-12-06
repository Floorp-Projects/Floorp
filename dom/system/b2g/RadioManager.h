/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
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
 * The Original Code is Telephony.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Ben Turner <bent.mozilla@gmail.com> (Original Author)
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

#ifndef mozilla_dom_telephony_radio_h__
#define mozilla_dom_telephony_radio_h__

#include "jsapi.h"
#include "nsComponentManagerUtils.h"
#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsServiceManagerUtils.h"

#include "nsIObserver.h"
#include "mozilla/ipc/Ril.h"

#define TELEPHONYRADIO_CONTRACTID "@mozilla.org/telephony/radio;1"
#define TELEPHONYRADIOINTERFACE_CONTRACTID "@mozilla.org/telephony/radio-interface;1"

#define BEGIN_TELEPHONY_NAMESPACE \
  namespace mozilla { namespace dom { namespace telephony {
#define END_TELEPHONY_NAMESPACE \
  } /* namespace telephony */ } /* namespace dom */ } /* namespace mozilla */
#define USING_TELEPHONY_NAMESPACE \
  using namespace mozilla::dom::telephony;

// {a5c3a6de-84c4-4b15-8611-8aeb8d97f8ba}
#define TELEPHONYRADIO_CID \
  {0xa5c3a6de, 0x84c4, 0x4b15, {0x86, 0x11, 0x8a, 0xeb, 0x8d, 0x97, 0xf8, 0xba}}

// {a688f191-8ffc-47f3-8740-94a312cf59cb}}
#define TELEPHONYRADIOINTERFACE_CID \
  {0xd66e7ece, 0x41b1, 0x4608, {0x82, 0x80, 0x72, 0x50, 0xa6, 0x44, 0xe6, 0x6f}}


class nsIXPConnectJSObjectHolder;
class nsITelephone;
class nsIWifi;

BEGIN_TELEPHONY_NAMESPACE

class RadioManager : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  nsresult Init();
  void Shutdown();

  static already_AddRefed<RadioManager>
  FactoryCreate();

  static already_AddRefed<nsITelephone>
  GetTelephone();

protected:
  RadioManager();
  ~RadioManager();

  nsresult InitTelephone(JSContext *cx);
  nsresult InitWifi(JSContext *cx);

  nsCOMPtr<nsITelephone> mTelephone;
  nsCOMPtr<nsIWifi> mWifi;
  bool mShutdown;
};

END_TELEPHONY_NAMESPACE

#endif // mozilla_dom_telephony_radio_h__
