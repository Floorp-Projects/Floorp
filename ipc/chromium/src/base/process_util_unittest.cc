// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#define _CRT_SECURE_NO_WARNINGS

#include "base/command_line.h"
#include "base/eintr_wrapper.h"
#include "base/file_path.h"
#include "base/multiprocess_test.h"
#include "base/path_service.h"
#include "base/platform_thread.h"
#include "base/process_util.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_LINUX)
#include <dlfcn.h>
#endif
#if defined(OS_POSIX)
#include <fcntl.h>
#include <sys/socket.h>
#endif
#if defined(OS_WIN)
#include <windows.h>
#endif

namespace base {

class ProcessUtilTest : public MultiProcessTest {
};

MULTIPROCESS_TEST_MAIN(SimpleChildProcess) {
  return 0;
}

TEST_F(ProcessUtilTest, SpawnChild) {
  ProcessHandle handle = this->SpawnChild(L"SimpleChildProcess");

  ASSERT_NE(static_cast<ProcessHandle>(NULL), handle);
  EXPECT_TRUE(WaitForSingleProcess(handle, 5000));
  base::CloseProcessHandle(handle);
}

MULTIPROCESS_TEST_MAIN(SlowChildProcess) {
  // Sleep until file "SlowChildProcess.die" is created.
  FILE *fp;
  do {
    PlatformThread::Sleep(100);
    fp = fopen("SlowChildProcess.die", "r");
  } while (!fp);
  fclose(fp);
  remove("SlowChildProcess.die");
  exit(0);
  return 0;
}

TEST_F(ProcessUtilTest, KillSlowChild) {
  remove("SlowChildProcess.die");
  ProcessHandle handle = this->SpawnChild(L"SlowChildProcess");
  ASSERT_NE(static_cast<ProcessHandle>(NULL), handle);
  FILE *fp = fopen("SlowChildProcess.die", "w");
  fclose(fp);
  EXPECT_TRUE(base::WaitForSingleProcess(handle, 5000));
  base::CloseProcessHandle(handle);
}

// TODO(estade): if possible, port these 2 tests.
#if defined(OS_WIN)
TEST_F(ProcessUtilTest, EnableLFH) {
  ASSERT_TRUE(EnableLowFragmentationHeap());
  if (IsDebuggerPresent()) {
    // Under these conditions, LFH can't be enabled. There's no point to test
    // anything.
    const char* no_debug_env = getenv("_NO_DEBUG_HEAP");
    if (!no_debug_env || strcmp(no_debug_env, "1"))
      return;
  }
  HANDLE heaps[1024] = { 0 };
  unsigned number_heaps = GetProcessHeaps(1024, heaps);
  EXPECT_GT(number_heaps, 0u);
  for (unsigned i = 0; i < number_heaps; ++i) {
    ULONG flag = 0;
    SIZE_T length;
    ASSERT_NE(0, HeapQueryInformation(heaps[i],
                                      HeapCompatibilityInformation,
                                      &flag,
                                      sizeof(flag),
                                      &length));
    // If flag is 0, the heap is a standard heap that does not support
    // look-asides. If flag is 1, the heap supports look-asides. If flag is 2,
    // the heap is a low-fragmentation heap (LFH). Note that look-asides are not
    // supported on the LFH.

    // We don't have any documented way of querying the HEAP_NO_SERIALIZE flag.
    EXPECT_LE(flag, 2u);
    EXPECT_NE(flag, 1u);
  }
}

TEST_F(ProcessUtilTest, CalcFreeMemory) {
  ProcessMetrics* metrics =
      ProcessMetrics::CreateProcessMetrics(::GetCurrentProcess());
  ASSERT_TRUE(NULL != metrics);

  // Typical values here is ~1900 for total and ~1000 for largest. Obviously
  // it depends in what other tests have done to this process.
  FreeMBytes free_mem1 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem1));
  EXPECT_LT(10u, free_mem1.total);
  EXPECT_LT(10u, free_mem1.largest);
  EXPECT_GT(2048u, free_mem1.total);
  EXPECT_GT(2048u, free_mem1.largest);
  EXPECT_GE(free_mem1.total, free_mem1.largest);
  EXPECT_TRUE(NULL != free_mem1.largest_ptr);

  // Allocate 20M and check again. It should have gone down.
  const int kAllocMB = 20;
  char* alloc = new char[kAllocMB * 1024 * 1024];
  EXPECT_TRUE(NULL != alloc);

  size_t expected_total = free_mem1.total - kAllocMB;
  size_t expected_largest = free_mem1.largest;

  FreeMBytes free_mem2 = {0};
  EXPECT_TRUE(metrics->CalculateFreeMemory(&free_mem2));
  EXPECT_GE(free_mem2.total, free_mem2.largest);
  EXPECT_GE(expected_total, free_mem2.total);
  EXPECT_GE(expected_largest, free_mem2.largest);
  EXPECT_TRUE(NULL != free_mem2.largest_ptr);

  delete[] alloc;
  delete metrics;
}

TEST_F(ProcessUtilTest, GetAppOutput) {
  // Let's create a decently long message.
  std::string message;
  for (int i = 0; i < 1025; i++) {  // 1025 so it does not end on a kilo-byte
                                    // boundary.
    message += "Hello!";
  }

  FilePath python_runtime;
  ASSERT_TRUE(PathService::Get(base::DIR_SOURCE_ROOT, &python_runtime));
  python_runtime = python_runtime.Append(FILE_PATH_LITERAL("third_party"))
                                 .Append(FILE_PATH_LITERAL("python_24"))
                                 .Append(FILE_PATH_LITERAL("python.exe"));

  // You have to put every parameter between quotes, otherwise this won't work,
  // don't ask me why.
  std::wstring cmd_line = L"\"" + python_runtime.value() + L"\" " +
      L"\"-c\" \"import sys; sys.stdout.write('" + ASCIIToWide(message) +
      L"');\"";
  std::string output;
  ASSERT_TRUE(base::GetAppOutput(cmd_line, &output));
  EXPECT_EQ(message, output);

  // Let's make sure stderr is ignored.
  cmd_line = L"\"" + python_runtime.value() + L"\" " +
      L"\"-c\" \"import sys; sys.stderr.write('Hello!');\"";
  output.clear();
  ASSERT_TRUE(base::GetAppOutput(cmd_line, &output));
  EXPECT_EQ("", output);
}
#endif  // defined(OS_WIN)

#if defined(OS_POSIX)
// Returns the maximum number of files that a process can have open.
// Returns 0 on error.
int GetMaxFilesOpenInProcess() {
  struct rlimit rlim;
  if (getrlimit(RLIMIT_NOFILE, &rlim) != 0) {
    return 0;
  }

  // rlim_t is a uint64_t - clip to maxint. We do this since FD #s are ints
  // which are all 32 bits on the supported platforms.
  rlim_t max_int = static_cast<rlim_t>(std::numeric_limits<int32_t>::max());
  if (rlim.rlim_cur > max_int) {
    return max_int;
  }

  return rlim.rlim_cur;
}

const int kChildPipe = 20;  // FD # for write end of pipe in child process.
MULTIPROCESS_TEST_MAIN(ProcessUtilsLeakFDChildProcess) {
  // This child process counts the number of open FDs, it then writes that
  // number out to a pipe connected to the parent.
  int num_open_files = 0;
  int write_pipe = kChildPipe;
  int max_files = GetMaxFilesOpenInProcess();
  for (int i = STDERR_FILENO + 1; i < max_files; i++) {
    if (i != kChildPipe) {
      if (HANDLE_EINTR(close(i)) != -1) {
        LOG(WARNING) << "Leaked FD " << i;
        num_open_files += 1;
      }
    }
  }

  // InitLogging always opens a file at startup.
  int expected_num_open_fds = 1;
#if defined(OS_LINUX)
  // On Linux, '/etc/localtime' is opened before the test's main() enters.
  expected_num_open_fds += 1;
#endif  // defined(OS_LINUX)
  num_open_files -= expected_num_open_fds;

  int written = HANDLE_EINTR(write(write_pipe, &num_open_files,
                                   sizeof(num_open_files)));
  DCHECK_EQ(static_cast<size_t>(written), sizeof(num_open_files));
  HANDLE_EINTR(close(write_pipe));

  return 0;
}

TEST_F(ProcessUtilTest, FDRemapping) {
  // Open some files to check they don't get leaked to the child process.
  int fds[2];
  if (pipe(fds) < 0)
    NOTREACHED();
  int pipe_read_fd = fds[0];
  int pipe_write_fd = fds[1];

  // open some dummy fds to make sure they don't propogate over to the
  // child process.
  int dev_null = open("/dev/null", O_RDONLY);
  int sockets[2];
  socketpair(AF_UNIX, SOCK_STREAM, 0, sockets);

  file_handle_mapping_vector fd_mapping_vec;
  fd_mapping_vec.push_back(std::pair<int,int>(pipe_write_fd, kChildPipe));
  ProcessHandle handle = this->SpawnChild(L"ProcessUtilsLeakFDChildProcess",
                                          fd_mapping_vec,
                                          false);
  ASSERT_NE(static_cast<ProcessHandle>(NULL), handle);
  HANDLE_EINTR(close(pipe_write_fd));

  // Read number of open files in client process from pipe;
  int num_open_files = -1;
  ssize_t bytes_read =
      HANDLE_EINTR(read(pipe_read_fd, &num_open_files, sizeof(num_open_files)));
  ASSERT_EQ(bytes_read, static_cast<ssize_t>(sizeof(num_open_files)));

  // Make sure 0 fds are leaked to the client.
  ASSERT_EQ(0, num_open_files);

  EXPECT_TRUE(WaitForSingleProcess(handle, 1000));
  base::CloseProcessHandle(handle);
  HANDLE_EINTR(close(fds[0]));
  HANDLE_EINTR(close(sockets[0]));
  HANDLE_EINTR(close(sockets[1]));
  HANDLE_EINTR(close(dev_null));
}

TEST_F(ProcessUtilTest, GetAppOutput) {
  std::string output;
  EXPECT_TRUE(GetAppOutput(CommandLine(L"true"), &output));
  EXPECT_STREQ("", output.c_str());

  EXPECT_FALSE(GetAppOutput(CommandLine(L"false"), &output));

  std::vector<std::string> argv;
  argv.push_back("/bin/echo");
  argv.push_back("-n");
  argv.push_back("foobar42");
  EXPECT_TRUE(GetAppOutput(CommandLine(argv), &output));
  EXPECT_STREQ("foobar42", output.c_str());
}

#endif  // defined(OS_POSIX)

}  // namespace base
