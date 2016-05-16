/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

//
// Deal with the differences between Microsoft and GNU implemenations
// of hash_map. Allows all platforms to use |base::hash_map| and
// |base::hash_set|.
//  eg:
//   base::hash_map<int> my_map;
//   base::hash_set<int> my_set;
//

#ifndef BASE_HASH_TABLES_H_
#define BASE_HASH_TABLES_H_

#include "build/build_config.h"

#include "base/string16.h"

#if defined(COMPILER_MSVC) || (defined(ANDROID) && defined(_STLP_STD_NAME))
#ifdef COMPILER_MSVC
#pragma push_macro("_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS")
#define _SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS
#endif

// Suppress -Wshadow warnings from stlport headers.
#ifdef __GNUC__
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wshadow"
#  if MOZ_GCC_VERSION_AT_LEAST(4, 9, 0)
#    pragma GCC diagnostic ignored "-Wshadow-local"
#  endif
#endif

#include <hash_map>
#include <hash_set>

#ifdef __GNUC__
#  if MOZ_GCC_VERSION_AT_LEAST(4, 9, 0)
#    pragma GCC diagnostic pop // -Wshadow-local
#  endif
#  pragma GCC diagnostic pop // -Wshadow
#endif

#ifdef COMPILER_MSVC
#pragma pop_macro("_SILENCE_STDEXT_HASH_DEPRECATION_WARNINGS")
#endif
namespace base {
#ifdef ANDROID
using _STLP_STD_NAME::hash_map;
using _STLP_STD_NAME::hash_set;
#else
using stdext::hash_map;
using stdext::hash_set;
#endif
}
#elif defined(COMPILER_GCC)
// This is a hack to disable the gcc 4.4 warning about hash_map and hash_set
// being deprecated.  We can get rid of this when we upgrade to VS2008 and we
// can use <tr1/unordered_map> and <tr1/unordered_set>.
#ifdef __DEPRECATED
#define CHROME_OLD__DEPRECATED __DEPRECATED
#undef __DEPRECATED
#endif

#include <ext/hash_map>
#include <ext/hash_set>
#include <string>

#ifdef CHROME_OLD__DEPRECATED
#define __DEPRECATED CHROME_OLD__DEPRECATED
#undef CHROME_OLD__DEPRECATED
#endif

namespace base {
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;
}  // namespace base

namespace __gnu_cxx {

// The GNU C++ library provides identiy hash functions for many integral types,
// but not for |long long|.  This hash function will truncate if |size_t| is
// narrower than |long long|.  This is probably good enough for what we will
// use it for.

#define DEFINE_TRIVIAL_HASH(integral_type) \
    template<> \
    struct hash<integral_type> { \
      std::size_t operator()(integral_type value) const { \
        return static_cast<std::size_t>(value); \
      } \
    }

DEFINE_TRIVIAL_HASH(long long);
DEFINE_TRIVIAL_HASH(unsigned long long);

#undef DEFINE_TRIVIAL_HASH

// Implement string hash functions so that strings of various flavors can
// be used as keys in STL maps and sets.  The hash algorithm comes from the
// GNU C++ library, in <tr1/functional>.  It is duplicated here because GCC
// versions prior to 4.3.2 are unable to compile <tr1/functional> when RTTI
// is disabled, as it is in our build.

#define DEFINE_STRING_HASH(string_type) \
    template<> \
    struct hash<string_type> { \
      std::size_t operator()(const string_type& s) const { \
        std::size_t result = 0; \
        for (string_type::const_iterator i = s.begin(); i != s.end(); ++i) \
          result = (result * 131) + *i; \
        return result; \
      } \
    }

DEFINE_STRING_HASH(std::string);
DEFINE_STRING_HASH(std::wstring);

#if defined(WCHAR_T_IS_UTF32)
// If string16 and std::wstring are not the same type, provide a
// specialization for string16.
DEFINE_STRING_HASH(string16);
#endif  // WCHAR_T_IS_UTF32

#undef DEFINE_STRING_HASH

}  // namespace __gnu_cxx

#endif  // COMPILER

#endif  // BASE_HASH_TABLES_H_
