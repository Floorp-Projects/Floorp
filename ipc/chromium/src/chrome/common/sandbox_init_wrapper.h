// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_SANDBOX_INIT_WRAPPER_H_
#define CHROME_COMMON_SANDBOX_INIT_WRAPPER_H_

// Wraps the sandbox initialization and platform variables to consolodate
// the code and reduce the number of platform ifdefs elsewhere. The POSIX
// version of this wrapper is basically empty.

#include "build/build_config.h"

#include <string>

#include "base/basictypes.h"
#if defined(OS_WIN)
#include "sandbox/src/sandbox.h"
#endif

class CommandLine;

#if defined(OS_WIN)

class SandboxInitWrapper {
 public:
  SandboxInitWrapper() : broker_services_(), target_services_() { }
  // SetServices() needs to be called before InitializeSandbox() on Win32 with
  // the info received from the chrome exe main.
  void SetServices(sandbox::SandboxInterfaceInfo* sandbox_info);
  sandbox::BrokerServices* BrokerServices() const { return broker_services_; }
  sandbox::TargetServices* TargetServices() const { return target_services_; }

  // Initialize the sandbox for renderer and plug-in processes, depending on
  // the command line flags. The browser process is not sandboxed.
  void InitializeSandbox(const CommandLine& parsed_command_line,
                         const std::wstring& process_type);
 private:
  sandbox::BrokerServices* broker_services_;
  sandbox::TargetServices* target_services_;

  DISALLOW_COPY_AND_ASSIGN(SandboxInitWrapper);
};

#elif defined(OS_POSIX)

class SandboxInitWrapper {
 public:
  SandboxInitWrapper() { }

  // Initialize the sandbox for renderer and plug-in processes, depending on
  // the command line flags. The browser process is not sandboxed.
  void InitializeSandbox(const CommandLine& parsed_command_line,
                         const std::wstring& process_type);

#if defined(OS_MACOSX)
  // We keep the process type so we can configure the sandbox as needed.
 public:
  std::wstring ProcessType() const { return process_type_; }
 private:
  std::wstring process_type_;
#endif

 private:
  DISALLOW_COPY_AND_ASSIGN(SandboxInitWrapper);
};

#endif

#endif  // CHROME_COMMON_SANDBOX_INIT_WRAPPER_H_
