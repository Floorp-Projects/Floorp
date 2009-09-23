// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/child_process_info.h"

#include <limits>

#ifndef CHROMIUM_MOZILLA_BUILD
#include "app/l10n_util.h"
#endif
#include "base/logging.h"
#include "base/process_util.h"
#include "base/rand_util.h"
#include "base/string_util.h"
#ifndef CHROMIUM_MOZILLA_BUILD
#include "grit/generated_resources.h"
#endif

std::wstring ChildProcessInfo::GetTypeNameInEnglish(
    ChildProcessInfo::ProcessType type) {
  switch (type) {
    case BROWSER_PROCESS:
      return L"Browser";
    case RENDER_PROCESS:
      return L"Tab";
    case PLUGIN_PROCESS:
      return L"Plug-in";
    case WORKER_PROCESS:
      return L"Web Worker";
    case UNKNOWN_PROCESS:
      default:
      DCHECK(false) << "Unknown child process type!";
      return L"Unknown";
    }
}

std::wstring ChildProcessInfo::GetLocalizedTitle() const {
#ifdef CHROMIUM_MOZILLA_BUILD
  return name_;
#else
  std::wstring title = name_;
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS && title.empty())
    title = l10n_util::GetString(IDS_TASK_MANAGER_UNKNOWN_PLUGIN_NAME);

  int message_id;
  if (type_ == ChildProcessInfo::PLUGIN_PROCESS) {
    message_id = IDS_TASK_MANAGER_PLUGIN_PREFIX;
  } else if (type_ == ChildProcessInfo::WORKER_PROCESS) {
    message_id = IDS_TASK_MANAGER_WORKER_PREFIX;
  } else {
    DCHECK(false) << "Need localized name for child process type.";
    return title;
  }

  // Explicitly mark name as LTR if there is no strong RTL character,
  // to avoid the wrong concatenation result similar to "!Yahoo! Mail: the
  // best web-based Email: NIGULP", in which "NIGULP" stands for the Hebrew
  // or Arabic word for "plugin".
  l10n_util::AdjustStringForLocaleDirection(title, &title);
  return l10n_util::GetStringF(message_id, title);
#endif
}

ChildProcessInfo::ChildProcessInfo(ProcessType type) {
  // This constructor is only used by objects which derive from this class,
  // which means *this* is a real object that refers to a child process, and not
  // just a simple object that contains information about it.  So add it to our
  // list of running processes.
  type_ = type;
  pid_ = -1;
}


ChildProcessInfo::~ChildProcessInfo() {
}

std::wstring ChildProcessInfo::GenerateRandomChannelID(void* instance) {
  // Note: the string must start with the current process id, this is how
  // child processes determine the pid of the parent.
  // Build the channel ID.  This is composed of a unique identifier for the
  // parent browser process, an identifier for the child instance, and a random
  // component. We use a random component so that a hacked child process can't
  // cause denial of service by causing future named pipe creation to fail.
  return StringPrintf(L"%d.%x.%d",
                      base::GetCurrentProcId(), instance,
                      base::RandInt(0, std::numeric_limits<int>::max()));
}
