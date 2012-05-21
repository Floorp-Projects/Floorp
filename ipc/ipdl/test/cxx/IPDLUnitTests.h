/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla__ipdltest_IPDLUnitTests_h
#define mozilla__ipdltest_IPDLUnitTests_h 1

#include "base/message_loop.h"
#include "base/process.h"
#include "chrome/common/ipc_channel.h"

#include "nsIAppShell.h"

#include "nsCOMPtr.h"
#include "nsDebug.h"
#include "nsServiceManagerUtils.h" // do_GetService()
#include "nsWidgetsCID.h"       // NS_APPSHELL_CID
#include "nsXULAppAPI.h"


#define MOZ_IPDL_TESTFAIL_LABEL "TEST-UNEXPECTED-FAIL"
#define MOZ_IPDL_TESTPASS_LABEL "TEST-PASS"
#define MOZ_IPDL_TESTINFO_LABEL "TEST-INFO"


namespace mozilla {
namespace _ipdltest {

//-----------------------------------------------------------------------------
// both processes
const char* const IPDLUnitTestName();

// NB: these are named like the similar functions in
// xpcom/test/TestHarness.h.  The names should nominally be kept in
// sync.

inline void fail(const char* fmt, ...)
{
  va_list ap;

  fprintf(stderr, MOZ_IPDL_TESTFAIL_LABEL " | %s | ", IPDLUnitTestName());

  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);

  fputc('\n', stderr);

  NS_RUNTIMEABORT("failed test");
}

inline void passed(const char* fmt, ...)
{
  va_list ap;

  printf(MOZ_IPDL_TESTPASS_LABEL " | %s | ", IPDLUnitTestName());

  va_start(ap, fmt);
  vprintf(fmt, ap);
  va_end(ap);

  fputc('\n', stdout);
}

//-----------------------------------------------------------------------------
// parent process only

class IPDLUnitTestSubprocess;

extern void* gParentActor;
extern IPDLUnitTestSubprocess* gSubprocess;

void IPDLUnitTestMain(void* aData);

void QuitParent();

//-----------------------------------------------------------------------------
// child process only

extern void* gChildActor;

void IPDLUnitTestChildInit(IPC::Channel* transport,
                           base::ProcessHandle parent,
                           MessageLoop* worker);

void QuitChild();

} // namespace _ipdltest
} // namespace mozilla


#endif // ifndef mozilla__ipdltest_IPDLUnitTests_h
