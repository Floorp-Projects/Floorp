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

#ifndef BASE_HASH_TABLES_H__
#define BASE_HASH_TABLES_H__

#include "build/build_config.h"

#if defined(COMPILER_MSVC)
#include <hash_map>
#include <hash_set>
namespace base {
using stdext::hash_map;
using stdext::hash_set;
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

#ifdef CHROME_OLD__DEPRECATED
#define __DEPRECATED CHROME_OLD__DEPRECATED
#undef CHROME_OLD__DEPRECATED
#endif

#include <tr1/functional>
namespace base {
using __gnu_cxx::hash_map;
using __gnu_cxx::hash_set;
}

// Implement string hash functions so that strings of various flavors can
// be used as keys in STL maps and sets.
namespace __gnu_cxx {

template<>
struct hash<wchar_t*> {
  size_t operator()(const wchar_t* s) const {
    return std::tr1::hash<const wchar_t*>()(s);
  }
};

template<>
struct hash<const wchar_t*> {
  size_t operator()(const wchar_t* s) const {
    return std::tr1::hash<const wchar_t*>()(s);
  }
};

template<>
struct hash<std::wstring> {
  size_t operator()(const std::wstring& s) const {
    return std::tr1::hash<std::wstring>()(s);
  }
};

template<>
struct hash<const std::wstring> {
  size_t operator()(const std::wstring& s) const {
    return std::tr1::hash<std::wstring>()(s);
  }
};

template<>
struct hash<std::string> {
  size_t operator()(const std::string& s) const {
    return std::tr1::hash<std::string>()(s);
  }
};

template<>
struct hash<const std::string> {
  size_t operator()(const std::string& s) const {
    return std::tr1::hash<std::string>()(s);
  }
};

template<>
struct hash<long long> {
  size_t operator()(long long i) const {
    return std::tr1::hash<long>()((long) i);
  }
};

}

#endif

#endif  // BASE_HASH_TABLES_H__
