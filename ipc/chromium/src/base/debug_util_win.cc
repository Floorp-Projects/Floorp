// Copyright (c) 2006-2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug_util.h"

#include <windows.h>
#include <dbghelp.h>
#include <iostream>

#include "base/basictypes.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/singleton.h"

namespace {

// Minimalist key reader.
// Note: Does not use the CRT.
bool RegReadString(HKEY root, const wchar_t* subkey,
                   const wchar_t* value_name, wchar_t* buffer, int* len) {
  HKEY key = NULL;
  DWORD res = RegOpenKeyEx(root, subkey, 0, KEY_READ, &key);
  if (ERROR_SUCCESS != res || key == NULL)
    return false;

  DWORD type = 0;
  DWORD buffer_size = *len * sizeof(wchar_t);
  // We don't support REG_EXPAND_SZ.
  res = RegQueryValueEx(key, value_name, NULL, &type,
                        reinterpret_cast<BYTE*>(buffer), &buffer_size);
  if (ERROR_SUCCESS == res && buffer_size != 0 && type == REG_SZ) {
    // Make sure the buffer is NULL terminated.
    buffer[*len - 1] = 0;
    *len = lstrlen(buffer);
    RegCloseKey(key);
    return true;
  }
  RegCloseKey(key);
  return false;
}

// Replaces each "%ld" in input per a value. Not efficient but it works.
// Note: Does not use the CRT.
bool StringReplace(const wchar_t* input, int value, wchar_t* output,
                   int output_len) {
  memset(output, 0, output_len*sizeof(wchar_t));
  int input_len = lstrlen(input);

  for (int i = 0; i < input_len; ++i) {
    int current_output_len = lstrlen(output);

    if (input[i] == L'%' && input[i + 1] == L'l' && input[i + 2] == L'd') {
      // Make sure we have enough place left.
      if ((current_output_len + 12) >= output_len)
        return false;

      // Cheap _itow().
      wsprintf(output+current_output_len, L"%d", value);
      i += 2;
    } else {
      if (current_output_len >= output_len)
        return false;
      output[current_output_len] = input[i];
    }
  }
  return true;
}

// SymbolContext is a threadsafe singleton that wraps the DbgHelp Sym* family
// of functions.  The Sym* family of functions may only be invoked by one
// thread at a time.  SymbolContext code may access a symbol server over the
// network while holding the lock for this singleton.  In the case of high
// latency, this code will adversly affect performance.
//
// There is also a known issue where this backtrace code can interact
// badly with breakpad if breakpad is invoked in a separate thread while
// we are using the Sym* functions.  This is because breakpad does now
// share a lock with this function.  See this related bug:
//
//   http://code.google.com/p/google-breakpad/issues/detail?id=311
//
// This is a very unlikely edge case, and the current solution is to
// just ignore it.
class SymbolContext {
 public:
  static SymbolContext* Get() {
    // We use a leaky singleton because code may call this during process
    // termination.
    return
      Singleton<SymbolContext, LeakySingletonTraits<SymbolContext> >::get();
  }

  // Initializes the symbols for the process if it hasn't been done yet.
  // Subsequent calls will not reinitialize the symbol, but instead return
  // the error code from the first call.
  bool Init() {
    AutoLock lock(lock_);
    if (!initialized_) {
      process_ = GetCurrentProcess();

      // Defer symbol load until they're needed, use undecorated names, and
      // get line numbers.
      SymSetOptions(SYMOPT_DEFERRED_LOADS |
                    SYMOPT_UNDNAME |
                    SYMOPT_LOAD_LINES);
      if (SymInitialize(process_, NULL, TRUE)) {
        init_error_ = ERROR_SUCCESS;
      } else {
        init_error_ = GetLastError();
      }
    }

    initialized_ = true;
    return init_error_ == ERROR_SUCCESS;
  }

  // Returns the error code of a failed initialization.  This should only be
  // called if Init() has been called.  We do not LOG(FATAL) here because
  // this code is called might be triggered by a LOG(FATAL) itself.  Instead,
  // we log an ERROR, and return ERROR_INVALID_DATA.
  DWORD init_error() {
    if (!initialized_) {
      LOG(ERROR) << "Calling GetInitError() before Init() was called.  "
                 << "Returning ERROR_INVALID_DATA.";
      return ERROR_INVALID_DATA;
    }

    return init_error_;
  }

  // Returns the process this was initialized for.  This should only be
  // called if Init() has been called.  We LOG(ERROR) in this situation.
  // LOG(FATAL) is not used because this code is might be triggered 
  // by a LOG(FATAL) itself.
  HANDLE process() {
    if (!initialized_) {
      LOG(ERROR) << "Calling process() before Init() was called. "
                 << "Returning NULL.";
      return NULL;
    }

    return process_;
  }

  // For the given trace, attempts to resolve the symbols, and output a trace
  // to the ostream os.  The format for each line of the backtrace is:
  //
  //    <tab>SymbolName[0xAddress+Offset] (FileName:LineNo)
  // 
  // This function should only be called if Init() has been called.  We do not
  // LOG(FATAL) here because this code is called might be triggered by a
  // LOG(FATAL) itself.
  void OutputTraceToStream(const std::vector<void*>& trace, std::ostream* os) {
    AutoLock lock(lock_);

    for (size_t i = 0; (i < trace.size()) && os->good(); ++i) {
      const int kMaxNameLength = 256;
      DWORD_PTR frame = reinterpret_cast<DWORD_PTR>(trace[i]);

      // Code adapted from MSDN example:
      // http://msdn.microsoft.com/en-us/library/ms680578(VS.85).aspx
      ULONG64 buffer[
        (sizeof(SYMBOL_INFO) +
          kMaxNameLength * sizeof(wchar_t) +
          sizeof(ULONG64) - 1) /
        sizeof(ULONG64)];

      // Initialize symbol information retrieval structures.
      DWORD64 sym_displacement = 0;
      PSYMBOL_INFO symbol = reinterpret_cast<PSYMBOL_INFO>(&buffer[0]);
      symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
      symbol->MaxNameLen = kMaxNameLength;
      BOOL has_symbol = SymFromAddr(process(), frame,
                                    &sym_displacement, symbol);

      // Attempt to retrieve line number information.
      DWORD line_displacement = 0;
      IMAGEHLP_LINE64 line = {};
      line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
      BOOL has_line = SymGetLineFromAddr64(process(), frame,
                                           &line_displacement, &line);

      // Output the backtrace line.
      (*os) << "\t";
      if (has_symbol) {
        (*os) << symbol->Name << " [0x" << trace[i] << "+"
              << sym_displacement << "]";
      } else {
        // If there is no symbol informtion, add a spacer.
        (*os) << "(No symbol) [0x" << trace[i] << "]";
      }
      if (has_line) {
        (*os) << " (" << line.FileName << ":" << line.LineNumber << ")";
      }
      (*os) << "\n";
    }
  }

  SymbolContext()
    : initialized_(false),
      process_(NULL),
      init_error_(ERROR_SUCCESS) {
  }

 private:
  Lock lock_;
  bool initialized_;
  HANDLE process_;
  DWORD init_error_;

  DISALLOW_COPY_AND_ASSIGN(SymbolContext);
};

}  // namespace

// Note: Does not use the CRT.
bool DebugUtil::SpawnDebuggerOnProcess(unsigned process_id) {
  wchar_t reg_value[1026];
  int len = arraysize(reg_value);
  if (RegReadString(HKEY_LOCAL_MACHINE,
      L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug",
      L"Debugger", reg_value, &len)) {
    wchar_t command_line[1026];
    if (StringReplace(reg_value, process_id, command_line,
                      arraysize(command_line))) {
      // We don't mind if the debugger is present because it will simply fail
      // to attach to this process.
      STARTUPINFO startup_info = {0};
      startup_info.cb = sizeof(startup_info);
      PROCESS_INFORMATION process_info = {0};

      if (CreateProcess(NULL, command_line, NULL, NULL, FALSE, 0, NULL, NULL,
                        &startup_info, &process_info)) {
        CloseHandle(process_info.hThread);
        WaitForInputIdle(process_info.hProcess, 10000);
        CloseHandle(process_info.hProcess);
        return true;
      }
    }
  }
  return false;
}

// static
bool DebugUtil::BeingDebugged() {
  return ::IsDebuggerPresent() != 0;
}

// static
void DebugUtil::BreakDebugger() {
  __debugbreak();
}

StackTrace::StackTrace() {
  // From http://msdn.microsoft.com/en-us/library/bb204633(VS.85).aspx,
  // the sum of FramesToSkip and FramesToCapture must be less than 63,
  // so set it to 62.
  const int kMaxCallers = 62;

  void* callers[kMaxCallers];
  // TODO(ajwong): Migrate this to StackWalk64.
  int count = CaptureStackBackTrace(0, kMaxCallers, callers, NULL);
  if (count > 0) {
    trace_.resize(count);
    memcpy(&trace_[0], callers, sizeof(callers[0]) * count);
  } else {
    trace_.resize(0);
  }
}

void StackTrace::PrintBacktrace() {
  OutputToStream(&std::cerr);
}

void StackTrace::OutputToStream(std::ostream* os) {
  SymbolContext* context = SymbolContext::Get();

  if (context->Init() != ERROR_SUCCESS) {
    DWORD error = context->init_error();
    (*os) << "Error initializing symbols (" << error
          << ").  Dumping unresolved backtrace:\n";
    for (size_t i = 0; (i < trace_.size()) && os->good(); ++i) {
      (*os) << "\t" << trace_[i] << "\n";
    }
  } else {
    (*os) << "Backtrace:\n";
    context->OutputTraceToStream(trace_, os);
  }
}
