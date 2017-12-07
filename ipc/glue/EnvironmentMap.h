// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SANDBOXING_COMMON_ENVIRONMENTMAP_H_
#define SANDBOXING_COMMON_ENVIRONMENTMAP_H_

#include <map>
#include <memory>
#include <string>

namespace base {

#if defined(OS_WIN)

typedef std::wstring NativeEnvironmentString;
typedef std::map<NativeEnvironmentString, NativeEnvironmentString>
    EnvironmentMap;

#define ENVIRONMENT_LITERAL(x) L##x
#define ENVIRONMENT_STRING NS_ConvertUTF8toUTF16

// Returns a modified environment vector constructed from the given environment
// and the list of changes given in |changes|. Each key in the environment is
// matched against the first element of the pairs. In the event of a match, the
// value is replaced by the second of the pair, unless the second is empty, in
// which case the key-value is removed.
//
// This Windows version takes and returns a Windows-style environment block
// which is a concatenated list of null-terminated 16-bit strings. The end is
// marked by a double-null terminator. The size of the returned string will
// include the terminators.
NativeEnvironmentString AlterEnvironment(const wchar_t* env,
                                         const EnvironmentMap& changes);

#elif defined(OS_POSIX)

typedef std::string NativeEnvironmentString;
typedef std::map<NativeEnvironmentString, NativeEnvironmentString>
    EnvironmentMap;

#define ENVIRONMENT_LITERAL(x) x
#define ENVIRONMENT_STRING

// See general comments for the Windows version above.
//
// This Posix version takes and returns a Posix-style environment block, which
// is a null-terminated list of pointers to null-terminated strings. The
// returned array will have appended to it the storage for the array itself so
// there is only one pointer to manage, but this means that you can't copy the
// array without keeping the original around.
std::unique_ptr<char* []> AlterEnvironment(
    const char* const* env,
    const EnvironmentMap& changes);

#endif

}  // namespace base

#endif  // SANDBOXING_COMMON_ENVIRONMENTMAP_H_
