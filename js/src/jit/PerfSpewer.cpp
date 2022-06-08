/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "jit/PerfSpewer.h"

#include "mozilla/IntegerPrintfMacros.h"
#include "mozilla/Printf.h"

#include "jit/Jitdump.h"

#ifdef XP_UNIX
#  include <fcntl.h>
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

#if defined(XP_LINUX) && !defined(ANDROID) && defined(__GLIBC__)
#  include <dlfcn.h>
#  include <sys/syscall.h>
#  include <sys/types.h>
#  include <unistd.h>
#  define gettid() static_cast<pid_t>(syscall(__NR_gettid))
#endif

#ifdef JS_ION_PERF
#  include "jit/JitSpewer.h"
#  include "jit/LIR.h"
#  include "jit/MIR.h"
#endif

#include "js/Printf.h"
#include "vm/MutexIDs.h"

using namespace js;
using namespace js::jit;

#define PERF_MODE_NONE 1
#define PERF_MODE_FUNC 2

#ifdef JS_ION_PERF

static uint32_t PerfMode = 0;

static bool PerfChecked = false;

static FILE* JitDumpFilePtr = nullptr;
static void* mmap_address = nullptr;

static js::Mutex* PerfMutex;

static uint64_t GetMonotonicTimestamp() {
  struct timespec ts = {};
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

static uint32_t GetMachineEncoding() {
#  if defined(JS_CODEGEN_X86)
  return 3;  // EM_386
#  elif defined(JS_CODEGEN_X64)
  return 62;  // EM_X86_64
#  elif defined(JS_CODEGEN_ARM)
  return 40;  // EM_ARM
#  elif defined(JS_CODEGEN_ARM64)
  return 183;  // EM_AARCH64
#  elif defined(JS_CODEGEN_MIPS32)
  return 8;  // EM_MIPS
#  elif defined(JS_CODEGEN_MIPS64)
  return 8;  // EM_MIPS
#  else
  return 0;  // Unsupported
#  endif
}

namespace {
struct MOZ_RAII AutoFileLock {
  AutoFileLock() {
    if (!PerfEnabled()) {
      return;
    }
    PerfMutex->lock();
    MOZ_ASSERT(JitDumpFilePtr);
  }
  ~AutoFileLock() {
    MOZ_ASSERT(JitDumpFilePtr);
    fflush(JitDumpFilePtr);
    PerfMutex->unlock();
  }
};
}  // namespace

static void WriteToJitDumpFile(const void* addr, uint32_t size,
                               AutoFileLock& lock) {
  MOZ_RELEASE_ASSERT(JitDumpFilePtr);
  size_t rv = fwrite(addr, 1, size, JitDumpFilePtr);
  MOZ_RELEASE_ASSERT(rv == size);
}

static void writeJitDumpHeader(AutoFileLock& lock) {
  JitDumpHeader header = {};
  header.magic = 0x4A695444;
  header.version = 1;
  header.total_size = sizeof(header);
  header.elf_mach = GetMachineEncoding();
  header.pad1 = 0;
  header.pid = getpid();
  header.timestamp = GetMonotonicTimestamp();
  header.flags = 0;

  WriteToJitDumpFile(&header, sizeof(header), lock);
}

static bool openJitDump() {
  if (JitDumpFilePtr) {
    return true;
  }
  AutoFileLock lock;

  const ssize_t bufferSize = 256;
  char filenameBuffer[bufferSize];

  const char* spew_dir = ".";
  if (getenv("PERF_SPEW_DIR")) {
    spew_dir = getenv("PERF_SPEW_DIR");
  }

  if (snprintf(filenameBuffer, bufferSize, "%s/jit-%d.dump", spew_dir,
               getpid()) >= bufferSize) {
    return false;
  }

  MOZ_ASSERT(!JitDumpFilePtr);

  int fd = open(filenameBuffer, O_CREAT | O_TRUNC | O_RDWR, 0666);
  JitDumpFilePtr = fdopen(fd, "w+");

  if (!JitDumpFilePtr) {
    return false;
  }

  // We need to mmap the jitdump file for perf to find it.
  long page_size = sysconf(_SC_PAGESIZE);
  mmap_address =
      mmap(nullptr, page_size, PROT_READ | PROT_EXEC, MAP_PRIVATE, fd, 0);
  if (mmap_address == MAP_FAILED) {
    PerfMode = PERF_MODE_NONE;
    return false;
  }

  writeJitDumpHeader(lock);
  return true;
}

void js::jit::CheckPerf() {
  if (!PerfChecked) {
    const char* env = getenv("IONPERF");
    if (env == nullptr) {
      PerfMode = PERF_MODE_NONE;
      fprintf(stderr,
              "Warning: JIT perf reporting requires IONPERF set to \"func\".");
      fprintf(stderr, "Perf mapping will be deactivated.\n");
    } else if (!strcmp(env, "func")) {
      PerfMode = PERF_MODE_FUNC;
    } else {
      fprintf(stderr, "Use IONPERF=func to record at function granularity\n");
      fprintf(stderr, "\n");
      fprintf(stderr, "Be advised that using IONPERF will cause all scripts\n");
      fprintf(stderr, "to be leaked.\n");
      exit(0);
    }

    if (PerfMode != PERF_MODE_NONE) {
      PerfMutex = js_new<js::Mutex>(mutexid::PerfSpewer);
      if (!PerfMutex) {
        MOZ_CRASH("failed to allocate PerfMutex");
      }

      if (openJitDump()) {
        PerfChecked = true;
        return;
      }

      fprintf(stderr, "Failed to open perf map file.  Disabling IONPERF.\n");
      PerfMode = PERF_MODE_NONE;
    }
    PerfChecked = true;
  }
}

bool js::jit::PerfFuncEnabled() {
  MOZ_ASSERT(PerfMode);
  return PerfMode == PERF_MODE_FUNC;
}

void PerfSpewer::WriteJitDumpLoadRecord(char* function_name, JitCode* code,
                                        AutoFileLock& lock) {
  WriteJitDumpLoadRecord(function_name, reinterpret_cast<void*>(code->raw()),
                         code->instructionsSize(), lock);
}

void PerfSpewer::WriteJitDumpLoadRecord(char* function_name, void* code_addr,
                                        uint64_t code_size,
                                        AutoFileLock& lock) {
  static uint64_t codeIndex = 1;

  JitDumpLoadRecord record = {};

  record.header.id = JIT_CODE_LOAD;
  record.header.total_size =
      sizeof(record) + strlen(function_name) + 1 + code_size;
  record.header.timestamp = GetMonotonicTimestamp();
  record.pid = getpid();
  record.tid = gettid();
  record.vma = uint64_t(code_addr);
  record.code_addr = uint64_t(code_addr);
  record.code_size = code_size;
  record.code_index = codeIndex++;

  WriteToJitDumpFile(&record, sizeof(record), lock);
  WriteToJitDumpFile(function_name, strlen(function_name) + 1, lock);
  WriteToJitDumpFile(code_addr, code_size, lock);
}

void PerfSpewer::WriteJitDumpEntry(const char* desc, JSScript* script,
                                   JitCode* code, AutoFileLock& lock) {
  UniqueChars function_name =
      JS_smprintf("%s %s:%u:%u", desc, script->filename(), script->lineno(),
                  script->column());
  WriteJitDumpLoadRecord(function_name.get(), code, lock);
}

void IonPerfSpewer::writeProfile(JSScript* script, JitCode* code) {
  if (!PerfEnabled()) {
    return;
  }
  AutoFileLock lock;

  WriteJitDumpEntry("Ion", script, code, lock);
}

void BaselinePerfSpewer::writeProfile(JSScript* script, JitCode* code) {
  if (!PerfEnabled()) {
    return;
  }
  AutoFileLock lock;

  WriteJitDumpEntry("Baseline", script, code, lock);
}

void InlineCachePerfSpewer::writeProfile(JitCode* code, const char* name) {
  if (!PerfEnabled()) {
    return;
  }
  AutoFileLock lock;

  auto desc = mozilla::Smprintf<js::SystemAllocPolicy>("IC: %s", name);
  PerfSpewer::WriteJitDumpLoadRecord(desc.get(), code, lock);
}

void js::jit::writePerfSpewerJitCodeProfile(JitCode* code, const char* msg) {
  if (!code || !PerfEnabled()) {
    return;
  }
  AutoFileLock lock;

  size_t size = code->instructionsSize();
  if (size > 0) {
    UniqueChars desc = JS_smprintf("%s", msg);
    PerfSpewer::WriteJitDumpLoadRecord(desc.get(), code, lock);
  }
}

void js::jit::writePerfSpewerWasmMap(uintptr_t base, uintptr_t size,
                                     const char* filename,
                                     const char* annotation) {
  if (size == 0U || !PerfEnabled()) {
    return;
  }
  AutoFileLock lock;

  UniqueChars desc = JS_smprintf("%s: Function %s", filename, annotation);
  PerfSpewer::WriteJitDumpLoadRecord(desc.get(), reinterpret_cast<void*>(base),
                                     uint64_t(size), lock);
}

void js::jit::writePerfSpewerWasmFunctionMap(uintptr_t base, uintptr_t size,
                                             const char* filename,
                                             unsigned lineno,
                                             const char* funcName) {
  if (size == 0U || !PerfEnabled()) {
    return;
  }
  AutoFileLock lock;

  UniqueChars desc =
      JS_smprintf("%s:%u: Function %s", filename, lineno, funcName);
  PerfSpewer::WriteJitDumpLoadRecord(desc.get(), reinterpret_cast<void*>(base),
                                     uint64_t(size), lock);
}

#endif  // defined (JS_ION_PERF)
