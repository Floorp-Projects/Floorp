/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#define MOZ_IPDL_TESTS
#include "mozilla/Bootstrap.h"

#if defined(XP_WIN)
#include <windows.h>
#include "nsWindowsWMain.cpp"
#endif

using namespace mozilla;

int
main(int argc, char** argv)
{
    // the first argument specifies which IPDL test case/suite to load
    if (argc < 2)
        return 1;

    Bootstrap::UniquePtr bootstrap;
    XRE_GetBootstrap(bootstrap);
    if (!bootstrap) {
        return 2;
    }
    return bootstrap->XRE_RunIPDLTest(argc, argv);
}
