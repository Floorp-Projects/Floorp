// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file/namespace contains utility functions for enumerating, ending and
// computing statistics of processes.

#ifndef BASE_PROCESS_UTIL_H_
#define BASE_PROCESS_UTIL_H_

#include "base/basictypes.h"

#if defined(OS_WIN)
#include <windows.h>
#include <tlhelp32.h>
#elif defined(OS_LINUX)
#include <dirent.h>
#include <limits.h>
#include <sys/types.h>
#elif defined(OS_MACOSX)
#include <mach/mach.h>
#endif

#include <map>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/process.h"

#if defined(OS_WIN)
typedef PROCESSENTRY32 ProcessEntry;
typedef IO_COUNTERS IoCounters;
#elif defined(OS_POSIX)
// TODO(port): we should not rely on a Win32 structure.
struct ProcessEntry {
  int pid;
  int ppid;
  char szExeFile[NAME_MAX + 1];
};

struct IoCounters {
  unsigned long long ReadOperationCount;
  unsigned long long WriteOperationCount;
  unsigned long long OtherOperationCount;
  unsigned long long ReadTransferCount;
  unsigned long long WriteTransferCount;
  unsigned long long OtherTransferCount;
};

#include "base/file_descriptor_shuffle.h"
#endif

#if defined(OS_MACOSX)
struct kinfo_proc;
#endif

namespace base {

// These can be used in a 32-bit bitmask.
enum ProcessArchitecture {
  PROCESS_ARCH_I386 = 0x1,
  PROCESS_ARCH_X86_64 = 0x2,
  PROCESS_ARCH_PPC = 0x4,
  PROCESS_ARCH_ARM = 0x8
};

inline ProcessArchitecture GetCurrentProcessArchitecture()
{
  base::ProcessArchitecture currentArchitecture;
#if defined(ARCH_CPU_X86)
  currentArchitecture = base::PROCESS_ARCH_I386;
#elif defined(ARCH_CPU_X86_64)
  currentArchitecture = base::PROCESS_ARCH_X86_64;
#elif defined(ARCH_CPU_PPC)
  currentArchitecture = base::PROCESS_ARCH_PPC;
#elif defined(ARCH_CPU_ARMEL)
  currentArchitecture = base::PROCESS_ARCH_ARM;
#endif
  return currentArchitecture;
}

// A minimalistic but hopefully cross-platform set of exit codes.
// Do not change the enumeration values or you will break third-party
// installers.
enum {
  PROCESS_END_NORMAL_TERMINATON = 0,
  PROCESS_END_KILLED_BY_USER    = 1,
  PROCESS_END_PROCESS_WAS_HUNG  = 2
};

// Returns the id of the current process.
ProcessId GetCurrentProcId();

// Returns the ProcessHandle of the current process.
ProcessHandle GetCurrentProcessHandle();

// Converts a PID to a process handle. This handle must be closed by
// CloseProcessHandle when you are done with it. Returns true on success.
bool OpenProcessHandle(ProcessId pid, ProcessHandle* handle);

// Converts a PID to a process handle. On Windows the handle is opened
// with more access rights and must only be used by trusted code.
// You have to close returned handle using CloseProcessHandle. Returns true
// on success.
bool OpenPrivilegedProcessHandle(ProcessId pid, ProcessHandle* handle);

// Closes the process handle opened by OpenProcessHandle.
void CloseProcessHandle(ProcessHandle process);

// Returns the unique ID for the specified process. This is functionally the
// same as Windows' GetProcessId(), but works on versions of Windows before
// Win XP SP1 as well.
ProcessId GetProcId(ProcessHandle process);

#if defined(OS_POSIX)
// Sets all file descriptors to close on exec except for stdin, stdout
// and stderr.
// TODO(agl): remove this function
// WARNING: do not use. It's inherently race-prone in the face of
// multi-threading.
void SetAllFDsToCloseOnExec();
// Close all file descriptors, expect those which are a destination in the
// given multimap. Only call this function in a child process where you know
// that there aren't any other threads.
void CloseSuperfluousFds(const base::InjectiveMultimap& saved_map);
#endif

#if defined(OS_WIN)
// Runs the given application name with the given command line. Normally, the
// first command line argument should be the path to the process, and don't
// forget to quote it.
//
// If wait is true, it will block and wait for the other process to finish,
// otherwise, it will just continue asynchronously.
//
// Example (including literal quotes)
//  cmdline = "c:\windows\explorer.exe" -foo "c:\bar\"
//
// If process_handle is non-NULL, the process handle of the launched app will be
// stored there on a successful launch.
// NOTE: In this case, the caller is responsible for closing the handle so
//       that it doesn't leak!
bool LaunchApp(const std::wstring& cmdline,
               bool wait, bool start_hidden, ProcessHandle* process_handle);
#elif defined(OS_POSIX)
// Runs the application specified in argv[0] with the command line argv.
// Before launching all FDs open in the parent process will be marked as
// close-on-exec.  |fds_to_remap| defines a mapping of src fd->dest fd to
// propagate FDs into the child process.
//
// As above, if wait is true, execute synchronously. The pid will be stored
// in process_handle if that pointer is non-null.
//
// Note that the first argument in argv must point to the filename,
// and must be fully specified.
typedef std::vector<std::pair<int, int> > file_handle_mapping_vector;
bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               bool wait, ProcessHandle* process_handle);

typedef std::map<std::string, std::string> environment_map;
enum ChildPrivileges {
  UNPRIVILEGED,
  SAME_PRIVILEGES_AS_PARENT
};
bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               const environment_map& env_vars_to_set,
               ChildPrivileges privs,
               bool wait, ProcessHandle* process_handle,
               ProcessArchitecture arch=GetCurrentProcessArchitecture());
bool LaunchApp(const std::vector<std::string>& argv,
               const file_handle_mapping_vector& fds_to_remap,
               const environment_map& env_vars_to_set,
               bool wait, ProcessHandle* process_handle,
               ProcessArchitecture arch=GetCurrentProcessArchitecture());

#endif

// Executes the application specified by cl. This function delegates to one
// of the above two platform-specific functions.
bool LaunchApp(const CommandLine& cl,
               bool wait, bool start_hidden, ProcessHandle* process_handle);

#if defined(OS_WIN)
// Executes the application specified by |cmd_line| and copies the contents
// printed to the standard output to |output|, which should be non NULL.
// Blocks until the started process terminates.
// Returns true if the application was run successfully, false otherwise.
bool GetAppOutput(const std::wstring& cmd_line, std::string* output);
#elif defined(OS_POSIX)
// Executes the application specified by |cl| and wait for it to exit. Stores
// the output (stdout) in |output|. Redirects stderr to /dev/null. Returns true
// on success (application launched and exited cleanly, with exit code
// indicating success). |output| is modified only when the function finished
// successfully.
bool GetAppOutput(const CommandLine& cl, std::string* output);
#endif

// Used to filter processes by process ID.
class ProcessFilter {
 public:
  // Returns true to indicate set-inclusion and false otherwise.  This method
  // should not have side-effects and should be idempotent.
  virtual bool Includes(ProcessId pid, ProcessId parent_pid) const = 0;
  virtual ~ProcessFilter() { }
};

// Returns the number of processes on the machine that are running from the
// given executable name.  If filter is non-null, then only processes selected
// by the filter will be counted.
int GetProcessCount(const std::wstring& executable_name,
                    const ProcessFilter* filter);

// Attempts to kill all the processes on the current machine that were launched
// from the given executable name, ending them with the given exit code.  If
// filter is non-null, then only processes selected by the filter are killed.
// Returns false if all processes were able to be killed off, false if at least
// one couldn't be killed.
bool KillProcesses(const std::wstring& executable_name, int exit_code,
                   const ProcessFilter* filter);

// Attempts to kill the process identified by the given process
// entry structure, giving it the specified exit code. If |wait| is true, wait
// for the process to be actually terminated before returning.
// Returns true if this is successful, false otherwise.
bool KillProcess(ProcessHandle process, int exit_code, bool wait);
#if defined(OS_WIN)
bool KillProcessById(ProcessId process_id, int exit_code, bool wait);
#endif

// Get the termination status (exit code) of the process and return true if the
// status indicates the process crashed. |child_exited| is set to true iff the
// child process has terminated. (|child_exited| may be NULL.)
//
// On Windows, it is an error to call this if the process hasn't terminated
// yet. On POSIX, |child_exited| is set correctly since we detect terminate in
// a different manner on POSIX.
bool DidProcessCrash(bool* child_exited, ProcessHandle handle);

// Waits for process to exit. In POSIX systems, if the process hasn't been
// signaled then puts the exit code in |exit_code|; otherwise it's considered
// a failure. On Windows |exit_code| is always filled. Returns true on success,
// and closes |handle| in any case.
bool WaitForExitCode(ProcessHandle handle, int* exit_code);

// Wait for all the processes based on the named executable to exit.  If filter
// is non-null, then only processes selected by the filter are waited on.
// Returns after all processes have exited or wait_milliseconds have expired.
// Returns true if all the processes exited, false otherwise.
bool WaitForProcessesToExit(const std::wstring& executable_name,
                            int wait_milliseconds,
                            const ProcessFilter* filter);

// Wait for a single process to exit. Return true if it exited cleanly within
// the given time limit.
bool WaitForSingleProcess(ProcessHandle handle,
                          int wait_milliseconds);

// Returns true when |wait_milliseconds| have elapsed and the process
// is still running.
bool CrashAwareSleep(ProcessHandle handle, int wait_milliseconds);

// Waits a certain amount of time (can be 0) for all the processes with a given
// executable name to exit, then kills off any of them that are still around.
// If filter is non-null, then only processes selected by the filter are waited
// on.  Killed processes are ended with the given exit code.  Returns false if
// any processes needed to be killed, true if they all exited cleanly within
// the wait_milliseconds delay.
bool CleanupProcesses(const std::wstring& executable_name,
                      int wait_milliseconds,
                      int exit_code,
                      const ProcessFilter* filter);

// This class provides a way to iterate through the list of processes
// on the current machine that were started from the given executable
// name.  To use, create an instance and then call NextProcessEntry()
// until it returns false.
class NamedProcessIterator {
 public:
  NamedProcessIterator(const std::wstring& executable_name,
                       const ProcessFilter* filter);
  ~NamedProcessIterator();

  // If there's another process that matches the given executable name,
  // returns a const pointer to the corresponding PROCESSENTRY32.
  // If there are no more matching processes, returns NULL.
  // The returned pointer will remain valid until NextProcessEntry()
  // is called again or this NamedProcessIterator goes out of scope.
  const ProcessEntry* NextProcessEntry();

 private:
  // Determines whether there's another process (regardless of executable)
  // left in the list of all processes.  Returns true and sets entry_ to
  // that process's info if there is one, false otherwise.
  bool CheckForNextProcess();

  bool IncludeEntry();

  // Initializes a PROCESSENTRY32 data structure so that it's ready for
  // use with Process32First/Process32Next.
  void InitProcessEntry(ProcessEntry* entry);

  std::wstring executable_name_;

#if defined(OS_WIN)
  HANDLE snapshot_;
  bool started_iteration_;
#elif defined(OS_LINUX)
  DIR *procfs_dir_;
#elif defined(OS_MACOSX)
  std::vector<kinfo_proc> kinfo_procs_;
  size_t index_of_kinfo_proc_;
#endif
  ProcessEntry entry_;
  const ProcessFilter* filter_;

  DISALLOW_EVIL_CONSTRUCTORS(NamedProcessIterator);
};

// Working Set (resident) memory usage broken down by
// priv (private): These pages (kbytes) cannot be shared with any other process.
// shareable:      These pages (kbytes) can be shared with other processes under
//                 the right circumstances.
// shared :        These pages (kbytes) are currently shared with at least one
//                 other process.
struct WorkingSetKBytes {
  size_t priv;
  size_t shareable;
  size_t shared;
};

// Committed (resident + paged) memory usage broken down by
// private: These pages cannot be shared with any other process.
// mapped:  These pages are mapped into the view of a section (backed by
//          pagefile.sys)
// image:   These pages are mapped into the view of an image section (backed by
//          file system)
struct CommittedKBytes {
  size_t priv;
  size_t mapped;
  size_t image;
};

// Free memory (Megabytes marked as free) in the 2G process address space.
// total : total amount in megabytes marked as free. Maximum value is 2048.
// largest : size of the largest contiguous amount of memory found. It is
//   always smaller or equal to FreeMBytes::total.
// largest_ptr: starting address of the largest memory block.
struct FreeMBytes {
  size_t total;
  size_t largest;
  void* largest_ptr;
};

// Provides performance metrics for a specified process (CPU usage, memory and
// IO counters). To use it, invoke CreateProcessMetrics() to get an instance
// for a specific process, then access the information with the different get
// methods.
class ProcessMetrics {
 public:
  // Creates a ProcessMetrics for the specified process.
  // The caller owns the returned object.
  static ProcessMetrics* CreateProcessMetrics(ProcessHandle process);

  ~ProcessMetrics();

  // Returns the current space allocated for the pagefile, in bytes (these pages
  // may or may not be in memory).
  size_t GetPagefileUsage() const;
  // Returns the peak space allocated for the pagefile, in bytes.
  size_t GetPeakPagefileUsage() const;
  // Returns the current working set size, in bytes.
  size_t GetWorkingSetSize() const;
  // Returns private usage, in bytes. Private bytes is the amount
  // of memory currently allocated to a process that cannot be shared.
  // Note: returns 0 on unsupported OSes: prior to XP SP2.
  size_t GetPrivateBytes() const;
  // Fills a CommittedKBytes with both resident and paged
  // memory usage as per definition of CommittedBytes.
  void GetCommittedKBytes(CommittedKBytes* usage) const;
  // Fills a WorkingSetKBytes containing resident private and shared memory
  // usage in bytes, as per definition of WorkingSetBytes.
  bool GetWorkingSetKBytes(WorkingSetKBytes* ws_usage) const;

  // Computes the current process available memory for allocation.
  // It does a linear scan of the address space querying each memory region
  // for its free (unallocated) status. It is useful for estimating the memory
  // load and fragmentation.
  bool CalculateFreeMemory(FreeMBytes* free) const;

  // Returns the CPU usage in percent since the last time this method was
  // called. The first time this method is called it returns 0 and will return
  // the actual CPU info on subsequent calls.
  // Note that on multi-processor machines, the CPU usage value is for all
  // CPUs. So if you have 2 CPUs and your process is using all the cycles
  // of 1 CPU and not the other CPU, this method returns 50.
  int GetCPUUsage();

  // Retrieves accounting information for all I/O operations performed by the
  // process.
  // If IO information is retrieved successfully, the function returns true
  // and fills in the IO_COUNTERS passed in. The function returns false
  // otherwise.
  bool GetIOCounters(IoCounters* io_counters) const;

 private:
  explicit ProcessMetrics(ProcessHandle process);

  ProcessHandle process_;

  int processor_count_;

  // Used to store the previous times so we can compute the CPU usage.
  int64 last_time_;
  int64 last_system_time_;

  DISALLOW_EVIL_CONSTRUCTORS(ProcessMetrics);
};

// Enables low fragmentation heap (LFH) for every heaps of this process. This
// won't have any effect on heaps created after this function call. It will not
// modify data allocated in the heaps before calling this function. So it is
// better to call this function early in initialization and again before
// entering the main loop.
// Note: Returns true on Windows 2000 without doing anything.
bool EnableLowFragmentationHeap();

// Enable 'terminate on heap corruption' flag. Helps protect against heap
// overflow. Has no effect if the OS doesn't provide the necessary facility.
void EnableTerminationOnHeapCorruption();

// If supported on the platform, and the user has sufficent rights, increase
// the current process's scheduling priority to a high priority.
void RaiseProcessToHighPriority();

}  // namespace base

#endif  // BASE_PROCESS_UTIL_H_
