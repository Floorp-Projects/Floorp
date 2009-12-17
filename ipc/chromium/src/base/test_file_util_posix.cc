// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test_file_util.h"

#include <errno.h>
#include <fts.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <string>

#include "base/logging.h"
#include "base/file_path.h"
#include "base/file_util.h"
#include "base/string_util.h"

namespace file_util {

bool CopyRecursiveDirNoCache(const std::wstring& source_dir,
                             const std::wstring& dest_dir) {
  const FilePath from_path(FilePath::FromWStringHack(source_dir));
  const FilePath to_path(FilePath::FromWStringHack(dest_dir));

  char top_dir[PATH_MAX];
  if (base::strlcpy(top_dir, from_path.value().c_str(),
                    arraysize(top_dir)) >= arraysize(top_dir)) {
    return false;
  }

  char* dir_list[] = { top_dir, NULL };
  FTS* fts = fts_open(dir_list, FTS_PHYSICAL | FTS_NOSTAT, NULL);
  if (!fts) {
    LOG(ERROR) << "fts_open failed: " << strerror(errno);
    return false;
  }

  int error = 0;
  FTSENT* ent;
  while (!error && (ent = fts_read(fts)) != NULL) {
    // ent->fts_path is the source path, including from_path, so paste
    // the suffix after from_path onto to_path to create the target_path.
    std::string suffix(&ent->fts_path[from_path.value().size()]);
    // Strip the leading '/' (if any).
    if (!suffix.empty()) {
      DCHECK_EQ('/', suffix[0]);
      suffix.erase(0, 1);
    }
    const FilePath target_path = to_path.Append(suffix);
    switch (ent->fts_info) {
      case FTS_D:  // Preorder directory.
        // Try creating the target dir, continuing on it if it exists already.
        // Rely on the user's umask to produce correct permissions.
        if (mkdir(target_path.value().c_str(), 0777) != 0) {
          if (errno != EEXIST)
            error = errno;
        }
        break;
      case FTS_F:     // Regular file.
      case FTS_NSOK:  // File, no stat info requested.
        {
          errno = 0;
          FilePath source_path(ent->fts_path);
          if (CopyFile(source_path, target_path)) {
            bool success = EvictFileFromSystemCache(
                target_path.Append(source_path.BaseName()));
            DCHECK(success);
          } else {
            error = errno ? errno : EINVAL;
          }
        }
        break;
      case FTS_DP:   // Postorder directory.
      case FTS_DOT:  // "." or ".."
        // Skip it.
        continue;
      case FTS_DC:   // Directory causing a cycle.
        // Skip this branch.
        if (fts_set(fts, ent, FTS_SKIP) != 0)
          error = errno;
        break;
      case FTS_DNR:  // Directory cannot be read.
      case FTS_ERR:  // Error.
      case FTS_NS:   // Stat failed.
        // Abort with the error.
        error = ent->fts_errno;
        break;
      case FTS_SL:      // Symlink.
      case FTS_SLNONE:  // Symlink with broken target.
        LOG(WARNING) << "skipping symbolic link: " << ent->fts_path;
        continue;
      case FTS_DEFAULT:  // Some other sort of file.
        LOG(WARNING) << "skipping file of unknown type: " << ent->fts_path;
        continue;
      default:
        NOTREACHED();
        continue;  // Hope for the best!
    }
  }
  // fts_read may have returned NULL and set errno to indicate an error.
  if (!error && errno != 0)
    error = errno;

  if (!fts_close(fts)) {
    // If we already have an error, let's use that error instead of the error
    // fts_close set.
    if (!error)
      error = errno;
  }

  if (error) {
    LOG(ERROR) << strerror(error);
    return false;
  }
  return true;
}

}  // namespace file_util
