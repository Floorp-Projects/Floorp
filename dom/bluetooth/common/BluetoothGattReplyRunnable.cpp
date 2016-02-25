/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "BluetoothGattReplyRunnable.h"
#include "mozilla/dom/bluetooth/BluetoothTypes.h"
#include "mozilla/dom/Promise.h"

using namespace mozilla::dom;

USING_BLUETOOTH_NAMESPACE

BluetoothGattReplyRunnable::BluetoothGattReplyRunnable(Promise* aPromise)
  : BluetoothReplyRunnable(nullptr, aPromise)
{
  MOZ_ASSERT(aPromise);
}

void
BluetoothGattReplyRunnable::GattStatusToDOMStatus(
  const BluetoothGattStatus aGattStatus, nsresult& aDOMStatus)
{
  /**
   * https://webbluetoothcg.github.io/web-bluetooth/#error-handling
   *
   * ToDo:
   * If the procedure times out or the ATT Bearer is terminated for any
   * reason, return |NS_ERROR_DOM_NETWORK_ERR|.
   */

  // Error Code Mapping
  if ((aGattStatus >= GATT_STATUS_BEGIN_OF_APPLICATION_ERROR) &&
      (aGattStatus <= GATT_STATUS_END_OF_APPLICATION_ERROR) &&
      IsWrite()) {
    aDOMStatus = NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
    return;
  }

  switch (aGattStatus) {
    case GATT_STATUS_INVALID_ATTRIBUTE_LENGTH:
      aDOMStatus = NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
      break;
    case GATT_STATUS_ATTRIBUTE_NOT_LONG:
      /**
       * ToDo:
       * While receiving |GATT_STATUS_ATTRIBUTE_NOT_LONG|, we need to check
       * whether 'Long' sub-procedure has been used or not.
       * If we have used 'Long' sub-procedure, we need to retry the step
       * without using 'Long' sub-procedure (e.g. Read Blob Request) based on
       * W3C reuqirements. If it fails again due to the length of the value
       * being written, convert the error status to
       * |NS_ERROR_DOM_INVALID_MODIFICATION_ERR|.
       * If 'Long' sub-procedure has not been used, convert the error status to
       * |NS_ERROR_DOM_NOT_SUPPORTED_ERR|.
       */
      aDOMStatus = NS_ERROR_DOM_INVALID_MODIFICATION_ERR;
      break;
    case GATT_STATUS_INSUFFICIENT_AUTHENTICATION:
    case GATT_STATUS_INSUFFICIENT_ENCRYPTION:
    case GATT_STATUS_INSUFFICIENT_ENCRYPTION_KEY_SIZE:
      /**
       * ToDo:
       * In W3C requirement, UA SHOULD attempt to increase the security level
       * of the connection while receiving those error statuses. If it fails or
       * UA doesn't suuport return the |NS_ERROR_DOM_SECURITY_ERR|.
       *
       * Note: The Gecko have already attempted to increase the security level
       * after receiving |GATT_STATUS_INSUFFICIENT_AUTHENTICATION| or
       * |GATT_STATUS_INSUFFICIENT_ENCRYPTION|. Only need to handle
       * |GATT_STATUS_INSUFFICIENT_ENCRYPTION_KEY_SIZE| in the future.
       */
      aDOMStatus = NS_ERROR_DOM_SECURITY_ERR;
      break;
    case GATT_STATUS_INSUFFICIENT_AUTHORIZATION:
      aDOMStatus = NS_ERROR_DOM_SECURITY_ERR;
      break;
    case GATT_STATUS_INVALID_HANDLE:
    case GATT_STATUS_INVALID_PDU:
    case GATT_STATUS_INVALID_OFFSET:
    case GATT_STATUS_ATTRIBUTE_NOT_FOUND:
    case GATT_STATUS_UNSUPPORTED_GROUP_TYPE:
    case GATT_STATUS_READ_NOT_PERMITTED:
    case GATT_STATUS_WRITE_NOT_PERMITTED:
    case GATT_STATUS_REQUEST_NOT_SUPPORTED:
    case GATT_STATUS_PREPARE_QUEUE_FULL:
    case GATT_STATUS_INSUFFICIENT_RESOURCES:
    case GATT_STATUS_UNLIKELY_ERROR:
    default:
      aDOMStatus = NS_ERROR_DOM_NOT_SUPPORTED_ERR;
      break;
  }
}

nsresult
BluetoothGattReplyRunnable::FireErrorString()
{
  MOZ_ASSERT(mReply);

  if (!mPromise ||
      mReply->type() != BluetoothReply::TBluetoothReplyError ||
      mReply->get_BluetoothReplyError().errorStatus().type() !=
        BluetoothErrorStatus::TBluetoothGattStatus) {
    return BluetoothReplyRunnable::FireErrorString();
  }

  nsresult domStatus = NS_OK;

  GattStatusToDOMStatus(
    mReply->get_BluetoothReplyError().errorStatus().get_BluetoothGattStatus(),
    domStatus);
  nsresult rv = NS_ERROR_GENERATE_FAILURE(NS_ERROR_MODULE_DOM, domStatus);
  mPromise->MaybeReject(rv);

  return NS_OK;
}
