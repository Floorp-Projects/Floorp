/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdType_h
#define mozilla_dom_IdType_h

#include "ipc/IPCMessageUtils.h"

namespace IPC {
template <typename T>
struct ParamTraits;
}  // namespace IPC

namespace mozilla::dom {
class BrowsingContext;
class ContentParent;
class BrowserParent;

template <typename T>
class IdType {
  friend struct IPC::ParamTraits<IdType<T>>;

 public:
  IdType() : mId(0) {}
  explicit IdType(uint64_t aId) : mId(aId) {}

  operator uint64_t() const { return mId; }

  IdType& operator=(uint64_t aId) {
    mId = aId;
    return *this;
  }

  bool operator<(const IdType& rhs) { return mId < rhs.mId; }

 private:
  uint64_t mId;
};

using TabId = IdType<BrowserParent>;
using ContentParentId = IdType<ContentParent>;
}  // namespace mozilla::dom

namespace IPC {

template <typename T>
struct ParamTraits<mozilla::dom::IdType<T>> {
  using paramType = mozilla::dom::IdType<T>;

  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.mId);
  }

  static bool Read(MessageReader* aReader, paramType* aResult) {
    return ReadParam(aReader, &aResult->mId);
  }
};

}  // namespace IPC

#endif  // mozilla_dom_IdType_h
