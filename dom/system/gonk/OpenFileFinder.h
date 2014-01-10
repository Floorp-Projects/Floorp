/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_system_openfilefinder_h__
#define mozilla_system_openfilefinder_h__

#include "nsString.h"

#include <dirent.h>

namespace mozilla {
namespace system {

class OpenFileFinder
{
public:
  enum State
  {
    NEXT_PID,
    CHECK_FDS,
    DONE
  };
  class Info
  {
  public:
    nsCString mFileName;  // name of the the open file
    nsCString mAppName;   // App which has the file open (if it's a b2g app)
    pid_t     mPid;       // pid of the process which has the file open
    nsCString mComm;      // comm associated with pid
    nsCString mExe;       // executable name associated with pid
    bool      mIsB2gOrDescendant; // this is b2g/its descendant or not
  };

  OpenFileFinder(const nsACString& aPath, bool aCheckIsB2gOrDescendant = true);
  ~OpenFileFinder();

  bool First(Info* aInfo);  // Return the first open file
  bool Next(Info* aInfo);   // Return the next open file
  void Close();

private:

  void FillInfo(Info *aInfo, const nsACString& aPath);
  bool ReadSymLink(const nsACString& aSymLink, nsACString& aOutPath);
  bool PathMatches(const nsACString& aPath)
  {
    return Substring(aPath, 0, mPath.Length()).Equals(mPath);
  }

  State     mState;   // Keeps track of what we're doing.
  nsCString mPath;    // Only report files contained within this directory tree
  DIR*      mProcDir; // Used for scanning /proc
  DIR*      mFdDir;   // Used for scanning /proc/PID/fd
  int       mPid;     // PID currently being processed
  pid_t     mMyPid;   // PID of parent process, we assume we're running on it.
  bool      mCheckIsB2gOrDescendant; // Do we care about non-b2g process?
};

} // system
} // mozilla

#endif  // mozilla_system_nsvolume_h__
