/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTestTypes_h
#define mozilla__ipdltest_IPDLUnitTestTypes_h

#include "mozilla/ipc/ProtocolUtils.h"  // ActorDestroyReason

namespace mozilla {
namespace _ipdltest {

struct DirtyRect {
  int x;
  int y;
  int w;
  int h;
};

}  // namespace _ipdltest
}  // namespace mozilla

namespace IPC {
template <>
struct ParamTraits<mozilla::_ipdltest::DirtyRect> {
  typedef mozilla::_ipdltest::DirtyRect paramType;
  static void Write(MessageWriter* aWriter, const paramType& aParam) {
    WriteParam(aWriter, aParam.x);
    WriteParam(aWriter, aParam.y);
    WriteParam(aWriter, aParam.w);
    WriteParam(aWriter, aParam.h);
  }
  static bool Read(MessageReader* aReader, void** aIter, paramType* aResult) {
    return (ReadParam(aReader, aIter, &aResult->x) &&
            ReadParam(aReader, aIter, &aResult->y) &&
            ReadParam(aReader, aIter, &aResult->w) &&
            ReadParam(aReader, aIter, &aResult->h));
  }
};
}  // namespace IPC

#endif  // ifndef mozilla__ipdltest_IPDLUnitTestTypes_h
