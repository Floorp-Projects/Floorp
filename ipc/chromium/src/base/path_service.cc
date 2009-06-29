// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/path_service.h"

#ifdef OS_WIN
#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#endif

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/hash_tables.h"
#include "base/lock.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/string_util.h"

namespace base {
  bool PathProvider(int key, FilePath* result);
#if defined(OS_WIN)
  bool PathProviderWin(int key, FilePath* result);
#elif defined(OS_MACOSX)
  bool PathProviderMac(int key, FilePath* result);
#elif defined(OS_LINUX)
  bool PathProviderLinux(int key, FilePath* result);
#endif
}

namespace {

typedef base::hash_map<int, FilePath> PathMap;
typedef base::hash_set<int> PathSet;

// We keep a linked list of providers.  In a debug build we ensure that no two
// providers claim overlapping keys.
struct Provider {
  PathService::ProviderFunc func;
  struct Provider* next;
#ifndef NDEBUG
  int key_start;
  int key_end;
#endif
  bool is_static;
};

static Provider base_provider = {
  base::PathProvider,
  NULL,
#ifndef NDEBUG
  base::PATH_START,
  base::PATH_END,
#endif
  true
};

#ifdef OS_WIN
static Provider base_provider_win = {
  base::PathProviderWin,
  &base_provider,
#ifndef NDEBUG
  base::PATH_WIN_START,
  base::PATH_WIN_END,
#endif
  true
};
#endif

#ifdef OS_MACOSX
static Provider base_provider_mac = {
  base::PathProviderMac,
  &base_provider,
#ifndef NDEBUG
  base::PATH_MAC_START,
  base::PATH_MAC_END,
#endif
  true
};
#endif

#if defined(OS_LINUX)
static Provider base_provider_linux = {
  base::PathProviderLinux,
  &base_provider,
#ifndef NDEBUG
  base::PATH_LINUX_START,
  base::PATH_LINUX_END,
#endif
  true
};
#endif


struct PathData {
  Lock      lock;
  PathMap   cache;      // Track mappings from path key to path value.
  PathSet   overrides;  // Track whether a path has been overridden.
  Provider* providers;  // Linked list of path service providers.

  PathData() {
#if defined(OS_WIN)
    providers = &base_provider_win;
#elif defined(OS_MACOSX)
    providers = &base_provider_mac;
#elif defined(OS_LINUX)
    providers = &base_provider_linux;
#endif
  }

  ~PathData() {
    Provider* p = providers;
    while (p) {
      Provider* next = p->next;
      if (!p->is_static)
        delete p;
      p = next;
    }
  }
};

static PathData* GetPathData() {
  return Singleton<PathData>::get();
}

}  // namespace


// static
bool PathService::GetFromCache(int key, FilePath* result) {
  PathData* path_data = GetPathData();
  AutoLock scoped_lock(path_data->lock);

  // check for a cached version
  PathMap::const_iterator it = path_data->cache.find(key);
  if (it != path_data->cache.end()) {
    *result = it->second;
    return true;
  }
  return false;
}

// static
void PathService::AddToCache(int key, const FilePath& path) {
  PathData* path_data = GetPathData();
  AutoLock scoped_lock(path_data->lock);
  // Save the computed path in our cache.
  path_data->cache[key] = path;
}

// TODO(brettw): this function does not handle long paths (filename > MAX_PATH)
// characters). This isn't supported very well by Windows right now, so it is
// moot, but we should keep this in mind for the future.
// static
bool PathService::Get(int key, FilePath* result) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(result);
  DCHECK(key >= base::DIR_CURRENT);

  // special case the current directory because it can never be cached
  if (key == base::DIR_CURRENT)
    return file_util::GetCurrentDirectory(result);

  if (GetFromCache(key, result))
    return true;

  FilePath path;

  // search providers for the requested path
  // NOTE: it should be safe to iterate here without the lock
  // since RegisterProvider always prepends.
  Provider* provider = path_data->providers;
  while (provider) {
    if (provider->func(key, &path))
      break;
    DCHECK(path.empty()) << "provider should not have modified path";
    provider = provider->next;
  }

  if (path.empty())
    return false;

  AddToCache(key, path);

  *result = path;
  return true;
}

// static
bool PathService::Get(int key, std::wstring* result) {
  // Deprecated compatibility function.
  FilePath path;
  if (!Get(key, &path))
    return false;
  *result = path.ToWStringHack();
  return true;
}

bool PathService::IsOverridden(int key) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);

  AutoLock scoped_lock(path_data->lock);
  return path_data->overrides.find(key) != path_data->overrides.end();
}

bool PathService::Override(int key, const std::wstring& path) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(key > base::DIR_CURRENT) << "invalid path key";

  std::wstring file_path = path;
#if defined(OS_WIN)
  // On Windows we switch the current working directory to load plugins (at
  // least). That's not the case on POSIX.
  // Also, on POSIX, AbsolutePath fails if called on a non-existant path.
  if (!file_util::AbsolutePath(&file_path))
    return false;
#endif

  // make sure the directory exists:
  if (!file_util::CreateDirectory(file_path))
    return false;

  file_util::TrimTrailingSeparator(&file_path);

  AutoLock scoped_lock(path_data->lock);
  path_data->cache[key] = FilePath::FromWStringHack(file_path);
  path_data->overrides.insert(key);
  return true;
}

bool PathService::SetCurrentDirectory(const std::wstring& current_directory) {
  return file_util::SetCurrentDirectory(current_directory);
}

void PathService::RegisterProvider(ProviderFunc func, int key_start,
                                   int key_end) {
  PathData* path_data = GetPathData();
  DCHECK(path_data);
  DCHECK(key_end > key_start);

  AutoLock scoped_lock(path_data->lock);

  Provider* p;

#ifndef NDEBUG
  p = path_data->providers;
  while (p) {
    DCHECK(key_start >= p->key_end || key_end <= p->key_start) <<
      "path provider collision";
    p = p->next;
  }
#endif

  p = new Provider;
  p->is_static = false;
  p->func = func;
  p->next = path_data->providers;
#ifndef NDEBUG
  p->key_start = key_start;
  p->key_end = key_end;
#endif
  path_data->providers = p;
}
