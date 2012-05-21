/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTestTestSubprocess_h
#define mozilla__ipdltest_IPDLUnitTestTestSubprocess_h 1


#include "mozilla/ipc/GeckoChildProcessHost.h"

namespace mozilla {
namespace _ipdltest {
//-----------------------------------------------------------------------------

class IPDLUnitTestSubprocess : public mozilla::ipc::GeckoChildProcessHost
{
public:
  IPDLUnitTestSubprocess();
  ~IPDLUnitTestSubprocess();

  /**
   * Asynchronously launch the plugin process.
   */
  // Could override parent Launch, but don't need to here
  //bool Launch();

private:
  DISALLOW_EVIL_CONSTRUCTORS(IPDLUnitTestSubprocess);
};


} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_IPDLUnitTestTestSubprocess_h
