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

  static void Write(MessageWriter* aWriter, const ParamType& aParam) {
    WriteParam(aWriter, aParam.mSuffix);
    WriteParam(aWriter, aParam.mGroup);
    WriteParam(aWriter, aParam.mOrigin);
    WriteParam(aWriter, aParam.mPersistenceType);
    WriteParam(aWriter, aParam.mPersisted);
    WriteParam(aWriter, aParam.mLastAccessTime);
  }

  static bool Read(MessageReader* aReader, ParamType* aResult) {
    return ReadParam(aReader, &aResult->mSuffix) &&
           ReadParam(aReader, &aResult->mGroup) &&
           ReadParam(aReader, &aResult->mOrigin) &&
           ReadParam(aReader, &aResult->mPersistenceType) &&
           ReadParam(aReader, &aResult->mPersisted) &&
           ReadParam(aReader, &aResult->mLastAccessTime);
  }
};

template <>
struct ParamTraits<mozilla::OriginAttributesPattern> {
  typedef mozilla::OriginAttributesPattern paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mFirstPartyDomain);
    WriteParam(aWriter, aParam.mInIsolatedMozBrowser);
    WriteParam(aWriter, aParam.mPrivateBrowsingId);
    WriteParam(aWriter, aParam.mUserContextId);
    WriteParam(aWriter, aParam.mGeckoViewSessionContextId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mFirstPartyDomain) &&
           ReadParam(aReader, &aResult->mInIsolatedMozBrowser) &&
           ReadParam(aReader, &aResult->mPrivateBrowsingId) &&
           ReadParam(aReader, &aResult->mUserContextId) &&
           ReadParam(aReader, &aResult->mGeckoViewSessionContextId);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_quota_SerializationHelpers_h
