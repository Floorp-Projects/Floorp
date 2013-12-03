/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "OpenFileFinder.h"

#include "mozilla/FileUtils.h"
#include "nsPrintfCString.h"

#include <sys/stat.h>
#include <errno.h>

namespace mozilla {
namespace system {

OpenFileFinder::OpenFileFinder(const nsACString& aPath,
                               bool aCheckIsB2gOrDescendant /* = true */)
  : mPath(aPath),
    mProcDir(nullptr),
    mFdDir(nullptr),
    mPid(0),
    mMyPid(-1),
    mCheckIsB2gOrDescendant(aCheckIsB2gOrDescendant)
{
}

OpenFileFinder::~OpenFileFinder()
{
  Close();
}

bool
OpenFileFinder::First(OpenFileFinder::Info* aInfo)
{
  Close();

  mProcDir = opendir("/proc");
  if (!mProcDir) {
    return false;
  }
  mState = NEXT_PID;
  return Next(aInfo);
}

bool
OpenFileFinder::Next(OpenFileFinder::Info* aInfo)
{
  // NOTE: This function calls readdir and readlink, neither of which should
  //       block since we're using the proc filesystem, which is a purely
  // kernel in-memory filesystem and doesn't depend on external driver
  // behaviour.
  while (mState != DONE) {
    switch (mState) {
      case NEXT_PID: {
        struct dirent *pidEntry;
        pidEntry = readdir(mProcDir);
        if (!pidEntry) {
          mState = DONE;
          break;
        }
        char *endPtr;
        mPid = strtol(pidEntry->d_name, &endPtr, 10);
        if (mPid == 0 || *endPtr != '\0') {
          // Not a +ve number - ignore
          continue;
        }
        // We've found a /proc/PID directory. Scan open file descriptors.
        if (mFdDir) {
          closedir(mFdDir);
        }
        nsPrintfCString fdDirPath("/proc/%d/fd", mPid);
        mFdDir = opendir(fdDirPath.get());
        if (!mFdDir) {
          continue;
        }
        mState = CHECK_FDS;
      }
      // Fall through
      case CHECK_FDS: {
        struct dirent *fdEntry;
        while((fdEntry = readdir(mFdDir))) {
          if (!strcmp(fdEntry->d_name, ".") ||
              !strcmp(fdEntry->d_name, "..")) {
            continue;
          }
          nsPrintfCString fdSymLink("/proc/%d/fd/%s", mPid, fdEntry->d_name);
          nsCString resolvedPath;
          if (ReadSymLink(fdSymLink, resolvedPath) && PathMatches(resolvedPath)) {
            // We found an open file contained within the directory tree passed
            // into the constructor.
            FillInfo(aInfo, resolvedPath);
            // If sCheckIsB2gOrDescendant is set false, the caller cares about
            // all processes which have open files. If sCheckIsB2gOrDescendant
            // is set false, we only care about the b2g proccess or its descendants.
            if (!mCheckIsB2gOrDescendant || aInfo->mIsB2gOrDescendant) {
              return true;
            }
            LOG("Ignore process(%d), not a b2g process or its descendant.",
                aInfo->mPid);
          }
        }
        // We've checked all of the files for this pid, move onto the next one.
        mState = NEXT_PID;
        continue;
      }
      case DONE:
      default:
        mState = DONE;  // covers the default case
        break;
    }
  }
  return false;
}

void
OpenFileFinder::Close()
{
  if (mFdDir) {
    closedir(mFdDir);
  }
  if (mProcDir) {
    closedir(mProcDir);
  }
}

void
OpenFileFinder::FillInfo(OpenFileFinder::Info* aInfo, const nsACString& aPath)
{
  aInfo->mFileName = aPath;
  aInfo->mPid = mPid;
  nsPrintfCString exePath("/proc/%d/exe", mPid);
  ReadSymLink(exePath, aInfo->mExe);
  aInfo->mComm.Truncate();
  aInfo->mAppName.Truncate();
  nsPrintfCString statPath("/proc/%d/stat", mPid);
  nsCString statString;
  statString.SetLength(200);
  char *stat = statString.BeginWriting();
  if (!stat) {
    return;
  }
  ReadSysFile(statPath.get(), stat, statString.Length());
  // The stat line includes the comm field, surrounded by parenthesis.
  // However, the contents of the comm field itself is arbitrary and
  // and can include ')', so we search for the rightmost ) as being
  // the end of the comm field.
  char *closeParen = strrchr(stat, ')');
  if (!closeParen) {
    return;
  }
  char *openParen = strchr(stat, '(');
  if (!openParen) {
    return;
  }
  if (openParen >= closeParen) {
    return;
  }
  nsDependentCSubstring comm(&openParen[1], closeParen - openParen - 1);
  aInfo->mComm = comm;
  // There is a single character field after the comm and then
  // the parent pid (the field we're interested in).
  // ) X ppid
  // 01234
  int ppid = atoi(&closeParen[4]);
  // We assume that we're running in the parent process
  if (mMyPid == -1) {
    mMyPid = getpid();
  }

  if (mPid == mMyPid) {
    // This is chrome process
    aInfo->mIsB2gOrDescendant = true;
    DBG("Chrome process has open file(s)");
    return;
  }
  // For the rest (non-chrome process), we recursively check the ppid to know
  // it is a descendant of b2g or not. See bug 931456.
  while (ppid != mMyPid && ppid != 1) {
    DBG("Process(%d) is not forked from b2g(%d) or Init(1), keep looking",
        ppid, mMyPid);
    nsPrintfCString ppStatPath("/proc/%d/stat", ppid);
    ReadSysFile(ppStatPath.get(), stat, statString.Length());
    closeParen = strrchr(stat, ')');
    if (!closeParen) {
      return;
    }
    ppid = atoi(&closeParen[4]);
  }
  if (ppid == 1) {
    // This is a not a b2g process.
    DBG("Non-b2g process has open file(s)");
    aInfo->mIsB2gOrDescendant = false;
    return;
  }
  if (ppid == mMyPid) {
    // This is a descendant of b2g.
    DBG("Child process of chrome process has open file(s)");
    aInfo->mIsB2gOrDescendant = true;
  }

  // This looks like a content process. The comm field will be the
  // app name.
  aInfo->mAppName = aInfo->mComm;
}

bool
OpenFileFinder::ReadSymLink(const nsACString& aSymLink, nsACString& aOutPath)
{
  aOutPath.Truncate();
  const char *symLink = aSymLink.BeginReading();

  // Verify that we actually have a symlink.
  struct stat st;
  if (lstat(symLink, &st)) {
      return false;
  }
  if ((st.st_mode & S_IFMT) != S_IFLNK) {
      return false;
  }

  // Contrary to the documentation st.st_size doesn't seem to be a reliable
  // indication of the length when reading from /proc, so we use a fixed
  // size buffer instead.

  char resolvedSymLink[PATH_MAX];
  ssize_t pathLength = readlink(symLink, resolvedSymLink,
                                sizeof(resolvedSymLink) - 1);
  if (pathLength <= 0) {
      return false;
  }
  resolvedSymLink[pathLength] = '\0';
  aOutPath.Assign(resolvedSymLink);
  return true;
}

} // system
} // mozilla
