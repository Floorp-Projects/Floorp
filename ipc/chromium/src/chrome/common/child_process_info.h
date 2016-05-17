/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_CHILD_PROCESS_INFO_H_
#define CHROME_COMMON_CHILD_PROCESS_INFO_H_

#include <string>

#include "base/process.h"

// Holds information about a child process.
class ChildProcessInfo {
 public:
  enum ProcessType {
    BROWSER_PROCESS,
    RENDER_PROCESS,
    PLUGIN_PROCESS,
    WORKER_PROCESS,
    UNKNOWN_PROCESS
  };

  // Returns the type of the process.
  ProcessType type() const { return type_; }

  // Returns the name of the process.  i.e. for plugins it might be Flash, while
  // for workers it might be the domain that it's from.
  std::wstring name() const { return name_; }

  // Getter to the process handle.
  base::ProcessHandle handle() const { return process_.handle(); }

  virtual int GetProcessId() const {
    if (pid_ != -1)
      return pid_;

    pid_ = process_.pid();
    return pid_;
  }
  void SetProcessBackgrounded() const { process_.SetProcessBackgrounded(true); }

  // Returns an English name of the process type, should only be used for non
  // user-visible strings, or debugging pages like about:memory.
  static std::wstring GetTypeNameInEnglish(ProcessType type);

  // Returns a localized title for the child process.  For example, a plugin
  // process would be "Plug-in: Flash" when name is "Flash".
  std::wstring GetLocalizedTitle() const;

  ChildProcessInfo(const ChildProcessInfo& original) {
    type_ = original.type_;
    name_ = original.name_;
    process_ = original.process_;
    pid_ = original.pid_;
  }

  ChildProcessInfo& operator=(const ChildProcessInfo& original) {
    if (&original != this) {
      type_ = original.type_;
      name_ = original.name_;
      process_ = original.process_;
      pid_ = original.pid_;
    }
    return *this;
  }

  virtual ~ChildProcessInfo();

  // We define the < operator so that the ChildProcessInfo can be used as a key
  // in a std::map.
  bool operator <(const ChildProcessInfo& rhs) const {
    if (process_.handle() != rhs.process_.handle())
      return process_ .handle() < rhs.process_.handle();
    return false;
  }

  bool operator ==(const ChildProcessInfo& rhs) const {
    return process_.handle() == rhs.process_.handle();
  }

 protected:
  void set_type(ProcessType aType) { type_ = aType; }
  void set_name(const std::wstring& aName) { name_ = aName; }
  void set_handle(base::ProcessHandle aHandle) {
    process_.set_handle(aHandle);
    pid_ = -1;
  }

  // Derived objects need to use this constructor so we know what type we are.
  explicit ChildProcessInfo(ProcessType type);

 private:
  ProcessType type_;
  std::wstring name_;
  mutable int pid_;  // Cache of the process id.

  // The handle to the process.
  mutable base::Process process_;
};

#endif  // CHROME_COMMON_CHILD_PROCESS_INFO_H_
