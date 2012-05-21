/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTestThreadChild_h
#define mozilla__ipdltest_IPDLUnitTestThreadChild_h 1

#include "mozilla/ipc/ProcessChild.h"

namespace mozilla {
namespace _ipdltest {

class IPDLUnitTestProcessChild : public mozilla::ipc::ProcessChild
{
  typedef mozilla::ipc::ProcessChild ProcessChild;

public:
  IPDLUnitTestProcessChild(ProcessHandle aParentHandle) :
    ProcessChild(aParentHandle)
  { }

  ~IPDLUnitTestProcessChild()
  { }

  virtual bool Init();
};

} // namespace _ipdltest
} // namespace mozilla

#endif // ifndef mozilla__ipdltest_IPDLUnitTestThreadChild_h
