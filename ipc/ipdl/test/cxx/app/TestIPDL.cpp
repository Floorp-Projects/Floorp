/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsXULAppAPI.h"

#if defined(XP_WIN)
#include <windows.h>
#define XRE_DONT_SUPPORT_XPSP2 // this app doesn't ship
#include "nsWindowsWMain.cpp"
#endif

int
main(int argc, char** argv)
{
    // the first argument specifies which IPDL test case/suite to load
    if (argc < 2)
        return 1;

    return XRE_RunIPDLTest(argc, argv);
}
