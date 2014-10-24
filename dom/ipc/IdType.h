/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_IdType_h
#define mozilla_dom_IdType_h

#include "ipc/IPCMessageUtils.h"

namespace IPC {
template<typename T> struct ParamTraits;
}

namespace mozilla {
namespace dom {
class ContentParent;
class TabParent;


template<typename T>
class IdType
{

  friend struct IPC::ParamTraits<IdType<T>>;

public:
  IdType() : mId(0) {}
  explicit IdType(uint64_t aId) : mId(aId) {}

  operator uint64_t() const { return mId; }

  IdType& operator=(uint64_t aId)
  {
    mId = aId;
    return *this;
  }

  bool operator<(const IdType& rhs)
  {
    return mId < rhs.mId;
  }
private:
  uint64_t mId;
};

typedef IdType<TabParent> TabId;
typedef IdType<ContentParent> ContentParentId;

} // namespace dom
} // namespace mozilla

namespace IPC {

template<typename T>
struct ParamTraits<mozilla::dom::IdType<T>>
{
  typedef mozilla::dom::IdType<T> paramType;

  static void Write(Message* aMsg, const paramType& aParam)
  {
    WriteParam(aMsg, aParam.mId);
  }

  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return ReadParam(aMsg, aIter, &aResult->mId);
  }
};

}

#endif