// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/string_util.h"
#include "base/version.h"

// static
Version* Version::GetVersionFromString(const std::wstring& version_str) {
  if (!IsStringASCII(version_str))
    return NULL;
  return GetVersionFromString(WideToASCII(version_str));
}

// static
Version* Version::GetVersionFromString(const std::string& version_str) {
  Version* vers = new Version();
  if (vers->InitFromString(version_str))
    return vers;
  delete vers;
  return NULL;
}

bool Version::Equals(const Version& that) const {
  return CompareTo(that) == 0;
}

int Version::CompareTo(const Version& other) const {
  std::vector<uint16_t> other_components = other.components();
  size_t count = std::min(components_.size(), other_components.size());
  for (size_t i = 0; i < count; ++i) {
    if (components_[i] > other_components[i])
      return 1;
    if (components_[i] < other_components[i])
      return -1;
  }
  if (components_.size() > other_components.size()) {
    for (size_t i = count; i < components_.size(); ++i)
      if (components_[i] > 0)
        return 1;
  } else if (components_.size() < other_components.size()) {
    for (size_t i = count; i < other_components.size(); ++i)
      if (other_components[i] > 0)
        return -1;
  }
  return 0;
}

const std::string Version::GetString() const {
  std::string version_str;
  int count = components_.size();
  for (int i = 0; i < count - 1; ++i) {
    version_str.append(IntToString(components_[i]));
    version_str.append(".");
  }
  version_str.append(IntToString(components_[count - 1]));
  return version_str;
}

bool Version::InitFromString(const std::string& version_str) {
  std::vector<std::string> numbers;
  SplitString(version_str, '.', &numbers);
  for (std::vector<std::string>::iterator i = numbers.begin();
       i != numbers.end(); ++i) {
    int num;
    if (!StringToInt(*i, &num))
      return false;
    if (num < 0)
      return false;
    const uint16_t max = 0xFFFF;
    if (num > max)
      return false;
    // This throws out things like +3, or 032.
    if (IntToString(num) != *i)
      return false;
    uint16_t component = static_cast<uint16_t>(num);
    components_.push_back(component);
  }
  return true;
}
