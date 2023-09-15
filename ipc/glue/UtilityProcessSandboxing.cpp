/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
#include "UtilityProcessSandboxing.h"

#include <vector>
#include <string>

#include "prenv.h"

namespace mozilla::ipc {

std::vector<std::string> split(const std::string& str, char s) {
  std::vector<std::string> rv;
  size_t last = 0;
  size_t i;
  size_t c = str.size();
  for (i = 0; i <= c; ++i) {
    if (i == c || str[i] == s) {
      rv.push_back(str.substr(last, i - last));
      last = i + 1;
    }
  }
  return rv;
}

bool IsUtilitySandboxEnabled(const char* envVar, SandboxingKind aKind) {
  if (envVar == nullptr) {
    return true;
  }

  const std::string disableUtility(envVar);
  if (disableUtility == "1") {
    return false;
  }

  std::vector<std::string> components = split(disableUtility, ',');
  const std::string thisKind = "utility:" + std::to_string(aKind);
  for (const std::string& thisOne : components) {
    if (thisOne == thisKind) {
      return false;
    }
  }

  return true;
}

bool IsUtilitySandboxEnabled(SandboxingKind aKind) {
  return IsUtilitySandboxEnabled(PR_GetEnv("MOZ_DISABLE_UTILITY_SANDBOX"),
                                 aKind);
}

}  // namespace mozilla::ipc
