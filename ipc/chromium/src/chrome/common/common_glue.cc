// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "app/l10n_util.h"
#include "base/command_line.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "build/build_config.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/plugin/npobject_util.h"
#include "googleurl/src/url_util.h"
#include "webkit/glue/webkit_glue.h"

namespace webkit_glue {

bool GetExeDirectory(std::wstring *path) {
  return PathService::Get(base::DIR_EXE, path);
}

bool GetApplicationDirectory(std::wstring *path) {
  return PathService::Get(chrome::DIR_APP, path);
}

bool IsPluginRunningInRendererProcess() {
  return !IsPluginProcess();
}

std::wstring GetWebKitLocale() {
  // The browser process should have passed the locale to the renderer via the
  // --lang command line flag.  In single process mode, this will return the
  // wrong value.  TODO(tc): Fix this for single process mode.
  const CommandLine& parsed_command_line = *CommandLine::ForCurrentProcess();
  const std::wstring& lang =
      parsed_command_line.GetSwitchValue(switches::kLang);
  DCHECK(!lang.empty() ||
      (!parsed_command_line.HasSwitch(switches::kRendererProcess) &&
       !parsed_command_line.HasSwitch(switches::kPluginProcess)));
  return lang;
}

string16 GetLocalizedString(int message_id) {
  return WideToUTF16(l10n_util::GetString(message_id));
}

}  // namespace webkit_glue
