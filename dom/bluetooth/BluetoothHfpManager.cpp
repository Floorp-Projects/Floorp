/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothHfpManager.h"

#include "BluetoothReplyRunnable.h"
#include "BluetoothService.h"
#include "BluetoothServiceUuid.h"

#include "mozilla/Services.h"
#include "nsIObserverService.h"

USING_BLUETOOTH_NAMESPACE
using namespace mozilla::ipc;

static nsRefPtr<BluetoothHfpManager> sInstance = nullptr;

BluetoothHfpManager::BluetoothHfpManager() : mCurrentVgs(-1)
{
}

BluetoothHfpManager::~BluetoothHfpManager()
{
}

//static
BluetoothHfpManager*
BluetoothHfpManager::Get()
{
  MOZ_ASSERT(NS_IsMainThread());

  if (sInstance == nullptr) {
    sInstance = new BluetoothHfpManager();
  }

  return sInstance;
}

// Virtual function of class SocketConsumer
void
BluetoothHfpManager::ReceiveSocketData(UnixSocketRawData* aMessage)
{
  MOZ_ASSERT(NS_IsMainThread());

  const char* msg = (const char*)aMessage->mData;

  // For more information, please refer to 4.34.1 "Bluetooth Defined AT
  // Capabilities" in Bluetooth hands-free profile 1.6
  if (!strncmp(msg, "AT+BRSF=", 8)) {
    SendLine("+BRSF: 23");
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CIND=?", 9)) {
    nsAutoCString cindRange;

    cindRange += "+CIND: ";
    cindRange += "(\"battchg\",(0-5)),";
    cindRange += "(\"signal\",(0-5)),";
    cindRange += "(\"service\",(0,1)),";
    cindRange += "(\"call\",(0,1)),";
    cindRange += "(\"callsetup\",(0-3)),";
    cindRange += "(\"callheld\",(0-2)),";
    cindRange += "(\"roam\",(0,1))";

    SendLine(cindRange.get());
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CIND", 7)) {
    // FIXME - Bug 794349
    // This value reflects current status of telephony, roaming, battery ...,
    // so obviously fixed value must be wrong if there is an ongoing call. 
    // Need a patch for this, but currently just using fixed value for basic 
    // SLC establishment.
    SendLine("+CIND: 5,5,1,0,0,0,0");
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CMER=", 8)) {
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHLD=?", 9)) {
    SendLine("+CHLD: (0,1,2,3)");
    SendLine("OK");
  } else if (!strncmp(msg, "AT+CHLD=", 8)) {
    SendLine("OK");
  } else if (!strncmp(msg, "AT+VGS=", 7)) {
    // VGS range: [0, 15]
    int newVgs = msg[7] - '0';

    if (strlen(msg) > 8) {
      newVgs *= 10;
      newVgs += (msg[8] - '0');
    }

#ifdef DEBUG
    NS_ASSERTION(newVgs >= 0 && newVgs <= 15, "Received invalid VGS value");
#endif

    // Currently, we send volume up/down commands to represent that
    // volume has been changed by Bluetooth headset, and that will affect
    // the main stream volume of our device. In the future, we may want to
    // be able to set volume by stream.
    nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
    if (newVgs > mCurrentVgs) {
      os->NotifyObservers(nullptr, "bluetooth-volume-change", NS_LITERAL_STRING("up").get());
    } else if (newVgs < mCurrentVgs) {
      os->NotifyObservers(nullptr, "bluetooth-volume-change", NS_LITERAL_STRING("down").get());
    }

    mCurrentVgs = newVgs;

    SendLine("OK");
  } else {
#ifdef DEBUG
    nsCString warningMsg;
    warningMsg.AssignLiteral("Not handling HFP message, reply ok: ");
    warningMsg.Append(msg);
    NS_WARNING(warningMsg.get());
#endif
    SendLine("OK");
  }
}

bool
BluetoothHfpManager::Connect(const nsAString& aDeviceObjectPath,
                             BluetoothReplyRunnable* aRunnable)
{
  MOZ_ASSERT(NS_IsMainThread());

  BluetoothService* bs = BluetoothService::Get();
  if (!bs) {
    NS_WARNING("BluetoothService not available!");
    return false;
  }

  nsString serviceUuidStr =
    NS_ConvertUTF8toUTF16(mozilla::dom::bluetooth::BluetoothServiceUuidStr::Handsfree);

  nsRefPtr<BluetoothReplyRunnable> runnable = aRunnable;

  nsresult rv = bs->GetSocketViaService(aDeviceObjectPath,
                                        serviceUuidStr,
                                        BluetoothSocketType::RFCOMM,
                                        true,
                                        false,
                                        this,
                                        runnable);

  runnable.forget();
  return NS_FAILED(rv) ? false : true;
}

void
BluetoothHfpManager::Disconnect()
{
  CloseSocket();
}

bool
BluetoothHfpManager::SendLine(const char* aMessage)
{
  const char* kHfpCrlf = "\xd\xa";
  nsAutoCString msg;

  msg += kHfpCrlf;
  msg += aMessage;
  msg += kHfpCrlf;

  return SendSocketData(msg);
}

