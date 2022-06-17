/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_JSActorProtocolUtils_h
#define mozilla_dom_JSActorProtocolUtils_h

#include "mozilla/Assertions.h"   // MOZ_ASSERT
#include "mozilla/ErrorResult.h"  // ErrorResult

namespace mozilla {

namespace dom {

class JSActorProtocolUtils {
 public:
  template <typename ProtoT, typename ActorInfoT>
  static void FromIPCShared(ProtoT& aProto, const ActorInfoT& aInfo) {
    aProto->mRemoteTypes = aInfo.remoteTypes().Clone();

    if (aInfo.isESModule()) {
      aProto->mChild.mESModuleURI = aInfo.url();
    } else {
      aProto->mChild.mModuleURI = aInfo.url();
    }

    aProto->mChild.mObservers = aInfo.observers().Clone();
  }

  template <typename ProtoT, typename ActorInfoT>
  static void ToIPCShared(ActorInfoT& aInfo, const ProtoT& aProto) {
    aInfo.name() = aProto->mName;

    aInfo.remoteTypes() = aProto->mRemoteTypes.Clone();

    if (aProto->mChild.mModuleURI) {
      aInfo.url() = aProto->mChild.mModuleURI;
      aInfo.isESModule() = false;
    } else {
      aInfo.url() = aProto->mChild.mESModuleURI;
      aInfo.isESModule() = true;
    }

    aInfo.observers() = aProto->mChild.mObservers.Clone();
  }

  template <typename ProtoT, typename ActorOptionsT>
  static bool FromWebIDLOptionsShared(ProtoT& aProto,
                                      const ActorOptionsT& aOptions,
                                      ErrorResult& aRv) {
    if (aOptions.mRemoteTypes.WasPassed()) {
      MOZ_ASSERT(aOptions.mRemoteTypes.Value().Length());
      aProto->mRemoteTypes = aOptions.mRemoteTypes.Value();
    }

    if (aOptions.mParent.WasPassed()) {
      const auto& parentOptions = aOptions.mParent.Value();

      if (parentOptions.mModuleURI.WasPassed()) {
        if (parentOptions.mEsModuleURI.WasPassed()) {
          aRv.ThrowNotSupportedError(
              "moduleURI and esModuleURI are mutually exclusive.");
          return false;
        }

        aProto->mParent.mModuleURI.emplace(parentOptions.mModuleURI.Value());
      } else if (parentOptions.mEsModuleURI.WasPassed()) {
        aProto->mParent.mESModuleURI.emplace(
            parentOptions.mEsModuleURI.Value());
      } else {
        aRv.ThrowNotSupportedError(
            "Either moduleURI or esModuleURI is required.");
        return false;
      }
    }
    if (aOptions.mChild.WasPassed()) {
      const auto& childOptions = aOptions.mChild.Value();

      if (childOptions.mModuleURI.WasPassed()) {
        if (childOptions.mEsModuleURI.WasPassed()) {
          aRv.ThrowNotSupportedError(
              "moduleURI and esModuleURI are exclusive.");
          return false;
        }

        aProto->mChild.mModuleURI.emplace(childOptions.mModuleURI.Value());
      } else if (childOptions.mEsModuleURI.WasPassed()) {
        aProto->mChild.mESModuleURI.emplace(childOptions.mEsModuleURI.Value());
      } else {
        aRv.ThrowNotSupportedError(
            "Either moduleURI or esModuleURI is required.");
        return false;
      }
    }

    if (!aOptions.mChild.WasPassed() && !aOptions.mParent.WasPassed()) {
      aRv.ThrowNotSupportedError(
          "No point registering an actor with neither child nor parent "
          "specifications.");
      return false;
    }

    if (aOptions.mChild.WasPassed() &&
        aOptions.mChild.Value().mObservers.WasPassed()) {
      aProto->mChild.mObservers = aOptions.mChild.Value().mObservers.Value();
    }

    return true;
  }
};

}  // namespace dom
}  // namespace mozilla

#endif  // mozilla_dom_JSActorProtocolUtils_h
