/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef SerializedLoadContext_h
#define SerializedLoadContext_h

#include "base/basictypes.h"
#include "ipc/IPCMessageUtils.h"
#include "ipc/IPCMessageUtilsSpecializations.h"
#include "mozilla/BasePrincipal.h"

class nsILoadContext;

/*
 *  This file contains the IPC::SerializedLoadContext class, which is used to
 *  copy data across IPDL from Child process contexts so it is available in the
 *  Parent.
 */

class nsIChannel;
class nsIWebSocketChannel;

namespace IPC {

class SerializedLoadContext {
 public:
  SerializedLoadContext()
      : mIsNotNull(false),
        mIsPrivateBitValid(false),
        mIsContent(false),
        mUseRemoteTabs(false),
        mUseRemoteSubframes(false),
        mUseTrackingProtection(false) {
    Init(nullptr);
  }

  explicit SerializedLoadContext(nsILoadContext* aLoadContext);
  explicit SerializedLoadContext(nsIChannel* aChannel);
  explicit SerializedLoadContext(nsIWebSocketChannel* aChannel);

  void Init(nsILoadContext* aLoadContext);

  bool IsNotNull() const { return mIsNotNull; }
  bool IsPrivateBitValid() const { return mIsPrivateBitValid; }

  // used to indicate if child-side LoadContext * was null.
  bool mIsNotNull;
  // used to indicate if child-side mUsePrivateBrowsing flag is valid, even if
  // mIsNotNull is false, i.e., child LoadContext was null.
  bool mIsPrivateBitValid;
  bool mIsContent;
  bool mUseRemoteTabs;
  bool mUseRemoteSubframes;
  bool mUseTrackingProtection;
  mozilla::OriginAttributes mOriginAttributes;
};

// Function to serialize over IPDL
template <>
struct ParamTraits<SerializedLoadContext> {
  typedef SerializedLoadContext paramType;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    nsAutoCString suffix;
    aParam.mOriginAttributes.CreateSuffix(suffix);

    WriteParam(aWriter, aParam.mIsNotNull);
    WriteParam(aWriter, aParam.mIsContent);
    WriteParam(aWriter, aParam.mIsPrivateBitValid);
    WriteParam(aWriter, aParam.mUseRemoteTabs);
    WriteParam(aWriter, aParam.mUseRemoteSubframes);
    WriteParam(aWriter, aParam.mUseTrackingProtection);
    WriteParam(aWriter, suffix);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    nsAutoCString suffix;
    if (!ReadParam(aReader, &aResult->mIsNotNull) ||
        !ReadParam(aReader, &aResult->mIsContent) ||
        !ReadParam(aReader, &aResult->mIsPrivateBitValid) ||
        !ReadParam(aReader, &aResult->mUseRemoteTabs) ||
        !ReadParam(aReader, &aResult->mUseRemoteSubframes) ||
        !ReadParam(aReader, &aResult->mUseTrackingProtection) ||
        !ReadParam(aReader, &suffix)) {
      return false;
    }
    return aResult->mOriginAttributes.PopulateFromSuffix(suffix);
  }
};

}  // namespace IPC

#endif  // SerializedLoadContext_h
