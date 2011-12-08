/* -*- Mode: C++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 8 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla IPC.
 *
 * The Initial Developer of the Original Code is
 *   The Mozilla Foundation
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
