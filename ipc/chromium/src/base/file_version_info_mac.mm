// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_version_info.h"

#import <Cocoa/Cocoa.h>

#include "base/logging.h"
#include "base/string_util.h"

FileVersionInfo::FileVersionInfo(NSBundle *bundle) : bundle_(bundle) {
  [bundle_ retain];
}

FileVersionInfo::~FileVersionInfo() {
  [bundle_ release];
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfoForCurrentModule() {
  // TODO(erikkay): this should really use bundleForClass, but we don't have
  // a class to hang onto yet.
  NSBundle* bundle = [NSBundle mainBundle];
  return new FileVersionInfo(bundle);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const std::wstring& file_path) {
  NSString* path = [NSString stringWithCString:
      reinterpret_cast<const char*>(file_path.c_str())
        encoding:NSUTF32StringEncoding];
  return new FileVersionInfo([NSBundle bundleWithPath:path]);
}

// static
FileVersionInfo* FileVersionInfo::CreateFileVersionInfo(
    const FilePath& file_path) {
  NSString* path = [NSString stringWithUTF8String:file_path.value().c_str()];
  return new FileVersionInfo([NSBundle bundleWithPath:path]);
}

std::wstring FileVersionInfo::company_name() {
  return L"";
}

std::wstring FileVersionInfo::company_short_name() {
  return L"";
}

std::wstring FileVersionInfo::internal_name() {
  return L"";
}

std::wstring FileVersionInfo::product_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::product_short_name() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::comments() {
  return L"";
}

std::wstring FileVersionInfo::legal_copyright() {
  return GetStringValue(L"CFBundleGetInfoString");
}

std::wstring FileVersionInfo::product_version() {
  return GetStringValue(L"CFBundleShortVersionString");
}

std::wstring FileVersionInfo::file_description() {
  return L"";
}

std::wstring FileVersionInfo::legal_trademarks() {
  return L"";
}

std::wstring FileVersionInfo::private_build() {
  return L"";
}

std::wstring FileVersionInfo::file_version() {
  // CFBundleVersion has limitations that may not be honored by a
  // proper Chromium version number, so try KSVersion first.
  std::wstring version = GetStringValue(L"KSVersion");
  if (version == L"")
    version = GetStringValue(L"CFBundleVersion");
  return version;
}

std::wstring FileVersionInfo::original_filename() {
  return GetStringValue(L"CFBundleName");
}

std::wstring FileVersionInfo::special_build() {
  return L"";
}

std::wstring FileVersionInfo::last_change() {
  return L"";
}

bool FileVersionInfo::is_official_build() {
  return false;
}

bool FileVersionInfo::GetValue(const wchar_t* name, std::wstring* value_str) {
  if (bundle_) {
    NSString* value = [bundle_ objectForInfoDictionaryKey:
        [NSString stringWithUTF8String:WideToUTF8(name).c_str()]];
    if (value) {
      *value_str = reinterpret_cast<const wchar_t*>(
          [value cStringUsingEncoding:NSUTF32StringEncoding]);
      return true;
    }
  }
  return false;
}

std::wstring FileVersionInfo::GetStringValue(const wchar_t* name) {
  std::wstring str;
  if (GetValue(name, &str))
    return str;
  return L"";
}
