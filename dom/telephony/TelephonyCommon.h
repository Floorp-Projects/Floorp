/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_telephony_telephonycommon_h__
#define mozilla_dom_telephony_telephonycommon_h__

#include "mozilla/Attributes.h"
#include "mozilla/DOMEventTargetHelper.h"
#include "mozilla/ErrorResult.h"
#include "nsAutoPtr.h"
#include "nsCOMPtr.h"
#include "nsCycleCollectionParticipant.h"
#include "nsDebug.h"
#include "nsString.h"
#include "nsTArray.h"

#define BEGIN_TELEPHONY_NAMESPACE \
  namespace mozilla { namespace dom { namespace telephony {
#define END_TELEPHONY_NAMESPACE \
  } /* namespace telephony */ } /* namespace dom */ } /* namespace mozilla */
#define USING_TELEPHONY_NAMESPACE \
  using namespace mozilla::dom::telephony;

namespace mozilla {
namespace dom {

class CallsList;
class Telephony;
class TelephonyCall;
class TelephonyCallGroup;

} // namespace dom
} // namespace mozilla

#endif // mozilla_dom_telephony_telephonycommon_h__
