// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MULTIPROCESS_TEST_H__
#define BASE_MULTIPROCESS_TEST_H__

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/process_util.h"
#include "base/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"
#include "testing/platform_test.h"

#if defined(OS_POSIX)
#include <sys/types.h>
#include <unistd.h>
#endif

// Command line switch to invoke a child process rather than
// to run the normal test suite.
static const wchar_t kRunClientProcess[] = L"client";

// A MultiProcessTest is a test class which makes it easier to
// write a test which requires code running out of process.
//
// To create a multiprocess test simply follow these steps:
//
// 1) Derive your test from MultiProcessTest. Example:
//
//    class MyTest : public MultiProcessTest {
//    };
//
//    TEST_F(MyTest, TestCaseName) {
//      ...
//    }
//
// 2) Create a mainline function for the child processes and include
//    testing/multiprocess_func_list.h.
//    See the declaration of the MULTIPROCESS_TEST_MAIN macro
//    in that file for an example.
// 3) Call SpawnChild(L"foo"), where "foo" is the name of
//    the function you wish to run in the child processes.
// That's it!
//
class MultiProcessTest : public PlatformTest {
 protected:
  // Run a child process.
  // 'procname' is the name of a function which the child will
  // execute.  It must be exported from this library in order to
  // run.
  //
  // Example signature:
  //    extern "C" int __declspec(dllexport) FooBar() {
  //         // do client work here
  //    }
  //
  // Returns the handle to the child, or NULL on failure
  //
  // TODO(darin): re-enable this once we have base/debug_util.h
  // ProcessDebugFlags(&cl, DebugUtil::UNKNOWN, false);
  base::ProcessHandle SpawnChild(const std::wstring& procname) {
    return SpawnChild(procname, false);
  }

  base::ProcessHandle SpawnChild(const std::wstring& procname,
                                 bool debug_on_start) {
#if defined(OS_WIN)
    return SpawnChildImpl(procname, debug_on_start);
#elif defined(OS_POSIX)
    base::file_handle_mapping_vector empty_file_list;
    return SpawnChildImpl(procname, empty_file_list, debug_on_start);
#endif
  }

#if defined(OS_POSIX)
  base::ProcessHandle SpawnChild(
      const std::wstring& procname,
      const base::file_handle_mapping_vector& fds_to_map,
      bool debug_on_start) {
    return SpawnChildImpl(procname, fds_to_map, debug_on_start);
  }
#endif

 private:
#if defined(OS_WIN)
  base::ProcessHandle SpawnChildImpl(
      const std::wstring& procname,
      bool debug_on_start) {
    CommandLine cl(*CommandLine::ForCurrentProcess());
    base::ProcessHandle handle = static_cast<base::ProcessHandle>(NULL);
    cl.AppendSwitchWithValue(kRunClientProcess, procname);

    if (debug_on_start)
      cl.AppendSwitch(switches::kDebugOnStart);

    base::LaunchApp(cl, false, true, &handle);
    return handle;
  }
#elif defined(OS_POSIX)
  // TODO(port): with the CommandLine refactoring, this code is very similar
  // to the Windows code.  Investigate whether this can be made shorter.
  base::ProcessHandle SpawnChildImpl(
      const std::wstring& procname,
      const base::file_handle_mapping_vector& fds_to_map,
      bool debug_on_start) {
    CommandLine cl(*CommandLine::ForCurrentProcess());
    base::ProcessHandle handle = static_cast<base::ProcessHandle>(NULL);
    cl.AppendSwitchWithValue(kRunClientProcess, procname);

    if (debug_on_start)
      cl.AppendSwitch(switches::kDebugOnStart);

    base::LaunchApp(cl.argv(), fds_to_map, false, &handle);
    return handle;
  }
#endif
};

#endif  // BASE_MULTIPROCESS_TEST_H__
