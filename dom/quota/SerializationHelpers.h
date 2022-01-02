/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_quota_SerializationHelpers_h
#define mozilla_dom_quota_SerializationHelpers_h

#include "ipc/EnumSerializer.h"
#include "ipc/IPCMessageUtils.h"

#include "mozilla/dom/quota/Client.h"
#include "mozilla/dom/quota/CommonMetadata.h"
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
struct ParamTraits<mozilla::dom::quota::FullOriginMetadata> {
  using ParamType = mozilla::dom::quota::FullOriginMetadata;

  static void Write(Message* aMsg, const ParamType& aParam) {
    WriteParam(aMsg, aParam.mSuffix);
    WriteParam(aMsg, aParam.mGroup);
    WriteParam(aMsg, aParam.mOrigin);
    WriteParam(aMsg, aParam.mPersistenceType);
    WriteParam(aMsg, aParam.mPersisted);
    WriteParam(aMsg, aParam.mLastAccessTime);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   ParamType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mSuffix) &&
           ReadParam(aMsg, aIter, &aResult->mGroup) &&
           ReadParam(aMsg, aIter, &aResult->mOrigin) &&
           ReadParam(aMsg, aIter, &aResult->mPersistenceType) &&
           ReadParam(aMsg, aIter, &aResult->mPersisted) &&
           ReadParam(aMsg, aIter, &aResult->mLastAccessTime);
  }
};

template <>
struct ParamTraits<mozilla::OriginAttributesPattern> {
  typedef mozilla::OriginAttributesPattern paramType;

  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.mFirstPartyDomain);
    WriteParam(aMsg, aParam.mInIsolatedMozBrowser);
    WriteParam(aMsg, aParam.mPrivateBrowsingId);
    WriteParam(aMsg, aParam.mUserContextId);
    WriteParam(aMsg, aParam.mGeckoViewSessionContextId);
  }

  static bool Read(const Message* aMsg, PickleIterator* aIter,
                   paramType* aResult) {
    return ReadParam(aMsg, aIter, &aResult->mFirstPartyDomain) &&
           ReadParam(aMsg, aIter, &aResult->mInIsolatedMozBrowser) &&
           ReadParam(aMsg, aIter, &aResult->mPrivateBrowsingId) &&
           ReadParam(aMsg, aIter, &aResult->mUserContextId) &&
           ReadParam(aMsg, aIter, &aResult->mGeckoViewSessionContextId);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_quota_SerializationHelpers_h
