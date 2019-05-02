/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_SerializationHelpers_h
#define mozilla_dom_quota_SerializationHelpers_h

#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/PersistenceType.h"
#include "mozilla/OriginAttributes.h"

namespace IPC {

template <>
struct ParamTraits<mozilla::dom::quota::PersistenceType>
    : public ContiguousEnumSerializer<
          mozilla::dom::quota::PersistenceType,
          mozilla::dom::quota::PERSISTENCE_TYPE_PERSISTENT,
          mozilla::dom::quota::PERSISTENCE_TYPE_INVALID> {};

template <>
struct ParamTraits<mozilla::dom::quota::Client::Type>
    : public ContiguousEnumSerializer<mozilla::dom::quota::Client::Type,
                                      mozilla::dom::quota::Client::IDB,
                                      mozilla::dom::quota::Client::TYPE_MAX> {};

template <>
struct ParamTraits<mozilla::OriginAttributesPattern> {
  typedef mozilla::OriginAttributesPattern paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mFirstPartyDomain);
    WriteParam(aMsg, aParam.mInIsolatedMozBrowser);
    WriteParam(aMsg, aParam.mPrivateBrowsingId);
    WriteParam(aMsg, aParam.mUserContextId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mFirstPartyDomain) &&
           ReadParam(aMsg, aIter, &aResult->mInIsolatedMozBrowser) &&
           ReadParam(aMsg, aIter, &aResult->mPrivateBrowsingId) &&
           ReadParam(aMsg, aIter, &aResult->mUserContextId);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_quota_SerializationHelpers_h
