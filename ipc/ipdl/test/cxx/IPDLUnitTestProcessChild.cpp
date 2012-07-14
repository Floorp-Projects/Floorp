/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/ipc/IOThreadChild.h"

#include "IPDLUnitTestProcessChild.h"
#include "IPDLUnitTests.h"

#include "nsRegion.h"

using mozilla::ipc::IOThreadChild;

namespace mozilla {
namespace _ipdltest {

bool
IPDLUnitTestProcessChild::Init()
{
    IPDLUnitTestChildInit(IOThreadChild::channel(),
                          ParentHandle(),
                          IOThreadChild::message_loop());

    if (NS_FAILED(nsRegion::InitStatic()))
      return false;

    return true;
}

} // namespace _ipdltest
} // namespace mozilla
