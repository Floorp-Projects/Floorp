/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_ipc_SidedVariant_h
#define mozilla_ipc_SidedVariant_h

#include <variant>
#include "mozilla/Assertions.h"
#include "mozilla/Attributes.h"
#include "mozilla/ipc/ProtocolUtils.h"
#include "ipc/IPCMessageUtils.h"

namespace mozilla::ipc {

/**
 * Helper type used by IPDL structs and unions to hold actor pointers with a
 * dynamic side.
 *
 * When sent over IPC, ParentSide will be used for send/recv on parent actors,
 * and ChildSide will be used for send/recv on child actors.
 */
template <typename ParentSide, typename ChildSide>
struct SideVariant {
 private:
 public:
  SideVariant() = default;
  template <typename U,
            std::enable_if_t<std::is_convertible_v<U&&, ParentSide>, int> = 0>
  MOZ_IMPLICIT SideVariant(U&& aParent) : mParent(std::forward<U>(aParent)) {}
  template <typename U,
            std::enable_if_t<std::is_convertible_v<U&&, ChildSide>, int> = 0>
  MOZ_IMPLICIT SideVariant(U&& aChild) : mChild(std::forward<U>(aChild)) {}
  MOZ_IMPLICIT SideVariant(std::nullptr_t) {}

  MOZ_IMPLICIT SideVariant& operator=(ParentSide aParent) {
    mParent = aParent;
    mChild = nullptr;
    return *this;
  }
  MOZ_IMPLICIT SideVariant& operator=(ChildSide aChild) {
    mChild = aChild;
    mParent = nullptr;
    return *this;
  }
  MOZ_IMPLICIT SideVariant& operator=(std::nullptr_t) {
    mChild = nullptr;
    mParent = nullptr;
    return *this;
  }

  MOZ_IMPLICIT operator bool() const { return mParent || mChild; }

  bool IsNull() const { return !operator bool(); }
  bool IsParent() const { return mParent; }
  bool IsChild() const { return mChild; }

  ParentSide AsParent() const {
    MOZ_ASSERT(IsNull() || IsParent());
    return mParent;
  }
  ChildSide AsChild() const {
    MOZ_ASSERT(IsNull() || IsChild());
    return mChild;
  }

 private:
  // As the values are both pointers, this is the same size as a variant would
  // be, but has less risk of type confusion, and supports an overall `nullptr`
  // value which is neither parent nor child.
  ParentSide mParent = nullptr;
  ChildSide mChild = nullptr;
};

}  // namespace mozilla::ipc

namespace IPC {

template <typename ParentSide, typename ChildSide>
struct ParamTraits<mozilla::ipc::SideVariant<ParentSide, ChildSide>> {
  typedef mozilla::ipc::SideVariant<ParentSide, ChildSide> paramType;

  static void Write(IPC::MessageWriter* aWriter, const paramType& aParam) {
    if (!aWriter->GetActor()) {
      aWriter->FatalError("actor required to serialize this type");
      return;
    }

    if (aWriter->GetActor()->GetSide() == mozilla::ipc::ParentSide) {
      if (aParam && !aParam.IsParent()) {
        aWriter->FatalError("invalid side");
        return;
      }
      WriteParam(aWriter, aParam.AsParent());
    } else {
      if (aParam && !aParam.IsChild()) {
        aWriter->FatalError("invalid side");
        return;
      }
      WriteParam(aWriter, aParam.AsChild());
    }
  }

  static mozilla::Maybe<paramType> Read(IPC::MessageReader* aReader) {
    if (!aReader->GetActor()) {
      aReader->FatalError("actor required to deserialize this type");
      return mozilla::Nothing();
    }

    if (aReader->GetActor()->GetSide() == mozilla::ipc::ParentSide) {
      return ReadParam<ParentSide>(aReader);
    }
    return ReadParam<ChildSide>(aReader);
  }
};

}  // namespace IPC

#endif  // mozilla_ipc_SidedVariant_h
