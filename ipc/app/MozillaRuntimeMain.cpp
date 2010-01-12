/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Plugin App.
 *
 * The Initial Developer of the Original Code is
 *   Ben Turner <bent.mozilla@gmail.com>.
 * Portions created by the Initial Developer are Copyright (C) 2009
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Chris Jones <jones.chris.g@gmail.com>.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsXPCOM.h"
#include "nsXULAppAPI.h"

// FIXME/cjones testing
#if !defined(OS_WIN)
#include <unistd.h>
#endif

#ifdef XP_WIN
#include <windows.h>
// we want a wmain entry point
#include "nsWindowsWMain.cpp"
#endif

int
main(int argc, char* argv[])
{
#if defined(MOZ_CRASHREPORTER)
    if (argc < 2)
        return 1;
    const char* const crashReporterArg = argv[--argc];

#  if defined(XP_WIN)
    // on windows, |crashReporterArg| is the named pipe on which the
    // server is listening for requests, or "-" if crash reporting is
    // disabled.
    if (0 != strcmp("-", crashReporterArg)
        && !XRE_SetRemoteExceptionHandler(crashReporterArg))
        return 1;
#  elif defined(OS_LINUX)
    // on POSIX, |crashReporterArg| is "true" if crash reporting is
    // enabled, false otherwise
    if (0 != strcmp("false", crashReporterArg)
        && !XRE_SetRemoteExceptionHandler(NULL))
        return 1;
#  else
#    error "OOP crash reporting unsupported on this platform"
#  endif   
#endif // if defined(MOZ_CRASHREPORTER)

#if defined(XP_WIN) && defined(DEBUG_bent)
    MessageBox(NULL, L"Hi", L"Hi", MB_OK);
#endif

    GeckoProcessType proctype =
        XRE_StringToChildProcessType(argv[argc - 1]);

    nsresult rv = XRE_InitChildProcess(argc - 1, argv, proctype);
    NS_ENSURE_SUCCESS(rv, 1);

    return 0;
}
