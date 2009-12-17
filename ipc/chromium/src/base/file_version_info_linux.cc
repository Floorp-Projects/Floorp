// Copyright (c) 2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"
#include "base/file_version_info_linux.h"

#include <string>

// TODO(mmoss) This only provides version info for the current binary, but it's
// also called for arbitrary files (e.g. plugins).
// See http://code.google.com/p/chromium/issues/detail?id=8132 for a discussion
// on what we should do with this module.

FileVersionInfo::FileVersionInfo() {}

FileVersionInfo::~FileVersionInfo() {}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  return new FileVersionInfo();
}

std::wstring FileVersionInfo::company_name() {
  return COMPANY_NAME;
}

std::wstring FileVersionInfo::company_short_name() {
  return COMPANY_SHORT_NAME;
}

std::wstring FileVersionInfo::product_name() {
  return PRODUCT_NAME;
}

std::wstring FileVersionInfo::product_short_name() {
  return PRODUCT_SHORT_NAME;
}

std::wstring FileVersionInfo::internal_name() {
  return INTERNAL_NAME;
}

std::wstring FileVersionInfo::product_version() {
  return PRODUCT_VERSION;
}

std::wstring FileVersionInfo::private_build() {
  return PRIVATE_BUILD;
}

std::wstring FileVersionInfo::special_build() {
  return SPECIAL_BUILD;
}

std::wstring FileVersionInfo::comments() {
  return COMMENTS;
}

std::wstring FileVersionInfo::original_filename() {
  return ORIGINAL_FILENAME;
}

std::wstring FileVersionInfo::file_description() {
  return FILE_DESCRIPTION;
}

std::wstring FileVersionInfo::file_version() {
  return FILE_VERSION;
}

std::wstring FileVersionInfo::legal_copyright() {
  return LEGAL_COPYRIGHT;
}

std::wstring FileVersionInfo::legal_trademarks() {
  return LEGAL_TRADEMARKS;
}

std::wstring FileVersionInfo::last_change() {
  return LAST_CHANGE;
}

bool FileVersionInfo::is_official_build() {
  return OFFICIAL_BUILD;
}
