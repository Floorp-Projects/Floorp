/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTestTypes_h
#define mozilla__ipdltest_IPDLUnitTestTypes_h

#include "mozilla/ipc/ProtocolUtils.h" // ActorDestroyReason

namespace mozilla {
namespace _ipdltest {

struct DirtyRect
{
  int x; int y; int w; int h;
};

}
}

namespace IPC {
template<>
struct ParamTraits<mozilla::_ipdltest::DirtyRect>
{
  typedef mozilla::_ipdltest::DirtyRect paramType;
  static void Write(Message* aMsg, const paramType& aParam) {
    WriteParam(aMsg, aParam.x);
    WriteParam(aMsg, aParam.y);
    WriteParam(aMsg, aParam.w);
    WriteParam(aMsg, aParam.h);
  }
  static bool Read(const Message* aMsg, void** aIter, paramType* aResult)
  {
    return (ReadParam(aMsg, aIter, &aResult->x) &&
            ReadParam(aMsg, aIter, &aResult->y) &&
            ReadParam(aMsg, aIter, &aResult->w) &&
            ReadParam(aMsg, aIter, &aResult->h));
  }
};
}


#endif // ifndef mozilla__ipdltest_IPDLUnitTestTypes_h
