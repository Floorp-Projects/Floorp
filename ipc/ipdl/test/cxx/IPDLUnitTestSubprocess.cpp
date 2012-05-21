/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "IPDLUnitTestSubprocess.h"

using mozilla::ipc::GeckoChildProcessHost;

namespace mozilla {
namespace _ipdltest {

IPDLUnitTestSubprocess::IPDLUnitTestSubprocess() :
    GeckoChildProcessHost(GeckoProcessType_IPDLUnitTest)
{
}

IPDLUnitTestSubprocess::~IPDLUnitTestSubprocess()
{
}

} // namespace _ipdltest
} // namespace mozilla
