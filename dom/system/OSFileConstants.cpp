/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/DebugOnly.h"

#include "fcntl.h"
#include "errno.h"

#include "prsystem.h"

#if defined(XP_UNIX)
#include "unistd.h"
#include "dirent.h"
#include "sys/stat.h"
#if defined(ANDROID)
#include <sys/vfs.h>
#define statvfs statfs
#else
#include "sys/statvfs.h"
#include <spawn.h>
#endif // defined(ANDROID)
#endif // defined(XP_UNIX)

#if defined(XP_LINUX)
#include <linux/fadvise.h>
#endif // defined(XP_LINUX)

#if defined(XP_MACOSX)
#include "copyfile.h"
#endif // defined(XP_MACOSX)

#if defined(XP_WIN)
#include <windows.h>
#include <accctrl.h>

#define PATH_MAX MAX_PATH

#endif // defined(XP_WIN)

#include "jsapi.h"
#include "jsfriendapi.h"
#include "BindingUtils.h"

// Used to provide information on the OS

#include "nsThreadUtils.h"
#include "nsIObserverService.h"
#include "nsIObserver.h"
#include "nsDirectoryServiceUtils.h"
#include "nsIXULRuntime.h"
#include "nsIPropertyBag2.h"
#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"
#include "nsAutoPtr.h"
#include "nsDirectoryServiceDefs.h"
#include "nsXULAppAPI.h"
#include "nsAppDirectoryServiceDefs.h"
#include "mozJSComponentLoader.h"

#include "OSFileConstants.h"
#include "nsIOSFileConstantsService.h"
#include "nsZipArchive.h"

#if defined(__DragonFly__) || defined(__FreeBSD__) \
  || defined(__NetBSD__) || defined(__OpenBSD__)
#define __dd_fd dd_fd
#endif

/**
 * This module defines the basic libc constants (error numbers, open modes,
 * etc.) used by OS.File and possibly other OS-bound JavaScript libraries.
 */


namespace mozilla {

// Use an anonymous namespace to hide the symbols and avoid any collision
// with, for instance, |extern bool gInitialized;|
namespace {
/**
 * |true| if this module has been initialized, |false| otherwise
 */
bool gInitialized = false;

struct Paths {
  /**
   * The name of the directory holding all the libraries (libxpcom, libnss, etc.)
   */
  nsString libDir;
  nsString tmpDir;
  nsString profileDir;
  nsString localProfileDir;
  /**
   * The user's home directory
   */
  nsString homeDir;
  /**
   * The user's desktop directory, if there is one. Otherwise this is
   * the same as homeDir.
   */
  nsString desktopDir;
  /**
   * The user's 'application data' directory.
   * Windows:
   *   HOME = Documents and Settings\$USER\Application Data
   *   UAppData = $HOME[\$vendor]\$name
   *
   * Unix:
   *   HOME = ~
   *   UAppData = $HOME/.[$vendor/]$name
   *
   * Mac:
   *   HOME = ~
   *   UAppData = $HOME/Library/Application Support/$name
   */
  nsString userApplicationDataDir;

#if defined(XP_WIN)
  /**
   * The user's application data directory.
   */
  nsString winAppDataDir;
  /**
   * The programs subdirectory in the user's start menu directory.
   */
  nsString winStartMenuProgsDir;
#endif // defined(XP_WIN)

#if defined(XP_MACOSX)
  /**
   * The user's Library directory.
   */
  nsString macUserLibDir;
  /**
   * The Application directory, that stores applications installed in the
   * system.
   */
  nsString macLocalApplicationsDir;
  /**
   * The user's trash directory.
   */
  nsString macTrashDir;
#endif // defined(XP_MACOSX)

  Paths()
  {
    libDir.SetIsVoid(true);
    tmpDir.SetIsVoid(true);
    profileDir.SetIsVoid(true);
    localProfileDir.SetIsVoid(true);
    homeDir.SetIsVoid(true);
    desktopDir.SetIsVoid(true);
    userApplicationDataDir.SetIsVoid(true);

#if defined(XP_WIN)
    winAppDataDir.SetIsVoid(true);
    winStartMenuProgsDir.SetIsVoid(true);
#endif // defined(XP_WIN)

#if defined(XP_MACOSX)
    macUserLibDir.SetIsVoid(true);
    macLocalApplicationsDir.SetIsVoid(true);
    macTrashDir.SetIsVoid(true);
#endif // defined(XP_MACOSX)
  }
};

/**
 * System directories.
 */
Paths* gPaths = nullptr;

/**
 * (Unix) the umask, which goes in OS.Constants.Sys but
 * can only be looked up (via the system-info service)
 * on the main thread.
 */
uint32_t gUserUmask = 0;
}

/**
 * Return the path to one of the special directories.
 *
 * @param aKey The key to the special directory (e.g. "TmpD", "ProfD", ...)
 * @param aOutPath The path to the special directory. In case of error,
 * the string is set to void.
 */
nsresult GetPathToSpecialDir(const char *aKey, nsString& aOutPath)
{
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(aKey, getter_AddRefs(file));
  if (NS_FAILED(rv) || !file) {
    return rv;
  }

  return file->GetPath(aOutPath);
}

/**
 * In some cases, OSFileConstants may be instantiated before the
 * profile is setup. In such cases, |OS.Constants.Path.profileDir| and
 * |OS.Constants.Path.localProfileDir| are undefined. However, we want
 * to ensure that this does not break existing code, so that future
 * workers spawned after the profile is setup have these constants.
 *
 * For this purpose, we register an observer to set |gPaths->profileDir|
 * and |gPaths->localProfileDir| once the profile is setup.
 */
class DelayedPathSetter final: public nsIObserver
{
  ~DelayedPathSetter() {}

  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  DelayedPathSetter() {}
};

NS_IMPL_ISUPPORTS(DelayedPathSetter, nsIObserver)

NS_IMETHODIMP
DelayedPathSetter::Observe(nsISupports*, const char * aTopic, const char16_t*)
{
  if (gPaths == nullptr) {
    // Initialization of gPaths has not taken place, something is wrong,
    // don't make things worse.
    return NS_OK;
  }
  nsresult rv = GetPathToSpecialDir(NS_APP_USER_PROFILE_50_DIR, gPaths->profileDir);
  if (NS_FAILED(rv)) {
    return rv;
  }
  rv = GetPathToSpecialDir(NS_APP_USER_PROFILE_LOCAL_50_DIR, gPaths->localProfileDir);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

/**
 * Perform the part of initialization that can only be
 * executed on the main thread.
 */
nsresult InitOSFileConstants()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (gInitialized) {
    return NS_OK;
  }

  gInitialized = true;

  nsAutoPtr<Paths> paths(new Paths);

  // Initialize paths->libDir
  nsCOMPtr<nsIFile> file;
  nsresult rv = NS_GetSpecialDirectory(NS_XPCOM_LIBRARY_FILE, getter_AddRefs(file));
  if (NS_FAILED(rv)) {
    return rv;
  }

  nsCOMPtr<nsIFile> libDir;
  rv = file->GetParent(getter_AddRefs(libDir));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = libDir->GetPath(paths->libDir);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Setup profileDir and localProfileDir immediately if possible (we
  // assume that NS_APP_USER_PROFILE_50_DIR and
  // NS_APP_USER_PROFILE_LOCAL_50_DIR are set simultaneously)
  rv = GetPathToSpecialDir(NS_APP_USER_PROFILE_50_DIR, paths->profileDir);
  if (NS_SUCCEEDED(rv)) {
    rv = GetPathToSpecialDir(NS_APP_USER_PROFILE_LOCAL_50_DIR, paths->localProfileDir);
  }

  // Otherwise, delay setup of profileDir/localProfileDir until they
  // become available.
  if (NS_FAILED(rv)) {
    nsCOMPtr<nsIObserverService> obsService = do_GetService(NS_OBSERVERSERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) {
      return rv;
    }
    nsRefPtr<DelayedPathSetter> pathSetter = new DelayedPathSetter();
    rv = obsService->AddObserver(pathSetter, "profile-do-change", false);
    if (NS_FAILED(rv)) {
      return rv;
    }
  }

  // For other directories, ignore errors (they may be undefined on
  // some platforms or in non-Firefox embeddings of Gecko).

  GetPathToSpecialDir(NS_OS_TEMP_DIR, paths->tmpDir);
  GetPathToSpecialDir(NS_OS_HOME_DIR, paths->homeDir);
  GetPathToSpecialDir(NS_OS_DESKTOP_DIR, paths->desktopDir);
  GetPathToSpecialDir(XRE_USER_APP_DATA_DIR, paths->userApplicationDataDir);

#if defined(XP_WIN)
  GetPathToSpecialDir(NS_WIN_APPDATA_DIR, paths->winAppDataDir);
  GetPathToSpecialDir(NS_WIN_PROGRAMS_DIR, paths->winStartMenuProgsDir);
#endif // defined(XP_WIN)

#if defined(XP_MACOSX)
  GetPathToSpecialDir(NS_MAC_USER_LIB_DIR, paths->macUserLibDir);
  GetPathToSpecialDir(NS_OSX_LOCAL_APPLICATIONS_DIR, paths->macLocalApplicationsDir);
  GetPathToSpecialDir(NS_MAC_TRASH_DIR, paths->macTrashDir);
#endif // defined(XP_MACOSX)

  gPaths = paths.forget();

  // Get the umask from the system-info service.
  // The property will always be present, but it will be zero on
  // non-Unix systems.
  nsCOMPtr<nsIPropertyBag2> infoService =
    do_GetService("@mozilla.org/system-info;1");
  MOZ_ASSERT(infoService, "Could not access the system information service");
  rv = infoService->GetPropertyAsUint32(NS_LITERAL_STRING("umask"),
                                        &gUserUmask);
  if (NS_FAILED(rv)) {
    return rv;
  }

  return NS_OK;
}

/**
 * Perform the cleaning up that can only be executed on the main thread.
 */
void CleanupOSFileConstants()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (!gInitialized) {
    return;
  }

  gInitialized = false;
  delete gPaths;
}


/**
 * Define a simple read-only property holding an integer.
 *
 * @param name The name of the constant. Used both as the JS name for the
 * constant and to access its value. Must be defined.
 *
 * Produces a |ConstantSpec|.
 */
#define INT_CONSTANT(name)      \
  { #name, INT_TO_JSVAL(name) }

/**
 * Define a simple read-only property holding an unsigned integer.
 *
 * @param name The name of the constant. Used both as the JS name for the
 * constant and to access its value. Must be defined.
 *
 * Produces a |ConstantSpec|.
 */
#define UINT_CONSTANT(name)      \
  { #name, UINT_TO_JSVAL((name)) }

/**
 * End marker for ConstantSpec
 */
#define PROP_END { nullptr, JS::UndefinedValue() }


// Define missing constants for Android
#if !defined(S_IRGRP)
#define S_IXOTH 0001
#define S_IWOTH 0002
#define S_IROTH 0004
#define S_IRWXO 0007
#define S_IXGRP 0010
#define S_IWGRP 0020
#define S_IRGRP 0040
#define S_IRWXG 0070
#define S_IXUSR 0100
#define S_IWUSR 0200
#define S_IRUSR 0400
#define S_IRWXU 0700
#endif // !defined(S_IRGRP)

/**
 * The properties defined in libc.
 *
 * If you extend this list of properties, please
 * separate categories ("errors", "open", etc.),
 * keep properties organized by alphabetical order
 * and #ifdef-away properties that are not portable.
 */
static const dom::ConstantSpec gLibcProperties[] =
{
  // Arguments for open
  INT_CONSTANT(O_APPEND),
  INT_CONSTANT(O_CREAT),
#if defined(O_DIRECTORY)
  INT_CONSTANT(O_DIRECTORY),
#endif // defined(O_DIRECTORY)
#if defined(O_EVTONLY)
  INT_CONSTANT(O_EVTONLY),
#endif // defined(O_EVTONLY)
  INT_CONSTANT(O_EXCL),
#if defined(O_EXLOCK)
  INT_CONSTANT(O_EXLOCK),
#endif // defined(O_EXLOCK)
#if defined(O_LARGEFILE)
  INT_CONSTANT(O_LARGEFILE),
#endif // defined(O_LARGEFILE)
#if defined(O_NOFOLLOW)
  INT_CONSTANT(O_NOFOLLOW),
#endif // defined(O_NOFOLLOW)
#if defined(O_NONBLOCK)
  INT_CONSTANT(O_NONBLOCK),
#endif // defined(O_NONBLOCK)
  INT_CONSTANT(O_RDONLY),
  INT_CONSTANT(O_RDWR),
#if defined(O_RSYNC)
  INT_CONSTANT(O_RSYNC),
#endif // defined(O_RSYNC)
#if defined(O_SHLOCK)
  INT_CONSTANT(O_SHLOCK),
#endif // defined(O_SHLOCK)
#if defined(O_SYMLINK)
  INT_CONSTANT(O_SYMLINK),
#endif // defined(O_SYMLINK)
#if defined(O_SYNC)
  INT_CONSTANT(O_SYNC),
#endif // defined(O_SYNC)
  INT_CONSTANT(O_TRUNC),
  INT_CONSTANT(O_WRONLY),

#if defined(AT_EACCESS)
  INT_CONSTANT(AT_EACCESS),
#endif //defined(AT_EACCESS)
#if defined(AT_FDCWD)
  INT_CONSTANT(AT_FDCWD),
#endif //defined(AT_FDCWD)
#if defined(AT_SYMLINK_NOFOLLOW)
  INT_CONSTANT(AT_SYMLINK_NOFOLLOW),
#endif //defined(AT_SYMLINK_NOFOLLOW)

#if defined(POSIX_FADV_SEQUENTIAL)
  INT_CONSTANT(POSIX_FADV_SEQUENTIAL),
#endif //defined(POSIX_FADV_SEQUENTIAL)

  // access
#if defined(F_OK)
  INT_CONSTANT(F_OK),
  INT_CONSTANT(R_OK),
  INT_CONSTANT(W_OK),
  INT_CONSTANT(X_OK),
#endif // defined(F_OK)

  // modes
  INT_CONSTANT(S_IRGRP),
  INT_CONSTANT(S_IROTH),
  INT_CONSTANT(S_IRUSR),
  INT_CONSTANT(S_IRWXG),
  INT_CONSTANT(S_IRWXO),
  INT_CONSTANT(S_IRWXU),
  INT_CONSTANT(S_IWGRP),
  INT_CONSTANT(S_IWOTH),
  INT_CONSTANT(S_IWUSR),
  INT_CONSTANT(S_IXOTH),
  INT_CONSTANT(S_IXGRP),
  INT_CONSTANT(S_IXUSR),

  // seek
  INT_CONSTANT(SEEK_CUR),
  INT_CONSTANT(SEEK_END),
  INT_CONSTANT(SEEK_SET),

 // fcntl command values
#if defined(XP_UNIX)
  INT_CONSTANT(F_GETLK),
  INT_CONSTANT(F_SETLK),
  INT_CONSTANT(F_SETLKW),

 // flock type values
  INT_CONSTANT(F_RDLCK),
  INT_CONSTANT(F_WRLCK),
  INT_CONSTANT(F_UNLCK),
#endif // defined(XP_UNIX)
  // copyfile
#if defined(COPYFILE_DATA)
  INT_CONSTANT(COPYFILE_DATA),
  INT_CONSTANT(COPYFILE_EXCL),
  INT_CONSTANT(COPYFILE_XATTR),
  INT_CONSTANT(COPYFILE_STAT),
  INT_CONSTANT(COPYFILE_ACL),
  INT_CONSTANT(COPYFILE_MOVE),
#endif // defined(COPYFILE_DATA)

  // error values
  INT_CONSTANT(EACCES),
  INT_CONSTANT(EAGAIN),
  INT_CONSTANT(EBADF),
  INT_CONSTANT(EEXIST),
  INT_CONSTANT(EFAULT),
  INT_CONSTANT(EFBIG),
  INT_CONSTANT(EINVAL),
  INT_CONSTANT(EIO),
  INT_CONSTANT(EISDIR),
#if defined(ELOOP) // not defined with VC9
  INT_CONSTANT(ELOOP),
#endif // defined(ELOOP)
  INT_CONSTANT(EMFILE),
  INT_CONSTANT(ENAMETOOLONG),
  INT_CONSTANT(ENFILE),
  INT_CONSTANT(ENOENT),
  INT_CONSTANT(ENOMEM),
  INT_CONSTANT(ENOSPC),
  INT_CONSTANT(ENOTDIR),
  INT_CONSTANT(ENXIO),
#if defined(EOPNOTSUPP) // not defined with VC 9
  INT_CONSTANT(EOPNOTSUPP),
#endif // defined(EOPNOTSUPP)
#if defined(EOVERFLOW) // not defined with VC 9
  INT_CONSTANT(EOVERFLOW),
#endif // defined(EOVERFLOW)
  INT_CONSTANT(EPERM),
  INT_CONSTANT(ERANGE),
#if defined(ETIMEDOUT) // not defined with VC 9
  INT_CONSTANT(ETIMEDOUT),
#endif // defined(ETIMEDOUT)
#if defined(EWOULDBLOCK) // not defined with VC 9
  INT_CONSTANT(EWOULDBLOCK),
#endif // defined(EWOULDBLOCK)
  INT_CONSTANT(EXDEV),

#if defined(DT_UNKNOWN)
  // Constants for |readdir|
  INT_CONSTANT(DT_UNKNOWN),
  INT_CONSTANT(DT_FIFO),
  INT_CONSTANT(DT_CHR),
  INT_CONSTANT(DT_DIR),
  INT_CONSTANT(DT_BLK),
  INT_CONSTANT(DT_REG),
  INT_CONSTANT(DT_LNK),
  INT_CONSTANT(DT_SOCK),
#endif // defined(DT_UNKNOWN)

#if defined(S_IFIFO)
  // Constants for |stat|
  INT_CONSTANT(S_IFMT),
  INT_CONSTANT(S_IFIFO),
  INT_CONSTANT(S_IFCHR),
  INT_CONSTANT(S_IFDIR),
  INT_CONSTANT(S_IFBLK),
  INT_CONSTANT(S_IFREG),
  INT_CONSTANT(S_IFLNK),
  INT_CONSTANT(S_IFSOCK),
#endif // defined(S_IFIFO)

  INT_CONSTANT(PATH_MAX),

  // Constants used to define data structures
  //
  // Many data structures have different fields/sizes/etc. on
  // various OSes / versions of the same OS / platforms. For these
  // data structures, we need to compute and export from C the size
  // and, if necessary, the offset of fields, so as to be able to
  // define the structure in JS.

#if defined(XP_UNIX)
  // The size of |mode_t|.
  { "OSFILE_SIZEOF_MODE_T", INT_TO_JSVAL(sizeof (mode_t)) },

  // The size of |gid_t|.
  { "OSFILE_SIZEOF_GID_T", INT_TO_JSVAL(sizeof (gid_t)) },

  // The size of |uid_t|.
  { "OSFILE_SIZEOF_UID_T", INT_TO_JSVAL(sizeof (uid_t)) },

  // The size of |time_t|.
  { "OSFILE_SIZEOF_TIME_T", INT_TO_JSVAL(sizeof (time_t)) },

  // The size of |fsblkcnt_t|.
  { "OSFILE_SIZEOF_FSBLKCNT_T", INT_TO_JSVAL(sizeof (fsblkcnt_t)) },

#if !defined(ANDROID)
  // The size of |posix_spawn_file_actions_t|.
  { "OSFILE_SIZEOF_POSIX_SPAWN_FILE_ACTIONS_T", INT_TO_JSVAL(sizeof (posix_spawn_file_actions_t)) },
#endif // !defined(ANDROID)

  // Defining |dirent|.
  // Size
  { "OSFILE_SIZEOF_DIRENT", INT_TO_JSVAL(sizeof (dirent)) },

  // Defining |flock|.
#if defined(XP_UNIX)
  { "OSFILE_SIZEOF_FLOCK", INT_TO_JSVAL(sizeof (struct flock)) },
  { "OSFILE_OFFSETOF_FLOCK_L_START", INT_TO_JSVAL(offsetof (struct flock, l_start)) },
  { "OSFILE_OFFSETOF_FLOCK_L_LEN", INT_TO_JSVAL(offsetof (struct flock, l_len)) },
  { "OSFILE_OFFSETOF_FLOCK_L_PID", INT_TO_JSVAL(offsetof (struct flock, l_pid)) },
  { "OSFILE_OFFSETOF_FLOCK_L_TYPE", INT_TO_JSVAL(offsetof (struct flock, l_type)) },
  { "OSFILE_OFFSETOF_FLOCK_L_WHENCE", INT_TO_JSVAL(offsetof (struct flock, l_whence)) },
#endif // defined(XP_UNIX)
  // Offset of field |d_name|.
  { "OSFILE_OFFSETOF_DIRENT_D_NAME", INT_TO_JSVAL(offsetof (struct dirent, d_name)) },
  // An upper bound to the length of field |d_name| of struct |dirent|.
  // (may not be exact, depending on padding).
  { "OSFILE_SIZEOF_DIRENT_D_NAME", INT_TO_JSVAL(sizeof (struct dirent) - offsetof (struct dirent, d_name)) },

  // Defining |timeval|.
  { "OSFILE_SIZEOF_TIMEVAL", INT_TO_JSVAL(sizeof (struct timeval)) },
  { "OSFILE_OFFSETOF_TIMEVAL_TV_SEC", INT_TO_JSVAL(offsetof (struct timeval, tv_sec)) },
  { "OSFILE_OFFSETOF_TIMEVAL_TV_USEC", INT_TO_JSVAL(offsetof (struct timeval, tv_usec)) },

#if defined(DT_UNKNOWN)
  // Position of field |d_type| in |dirent|
  // Not strictly posix, but seems defined on all platforms
  // except mingw32.
  { "OSFILE_OFFSETOF_DIRENT_D_TYPE", INT_TO_JSVAL(offsetof (struct dirent, d_type)) },
#endif // defined(DT_UNKNOWN)

  // Under MacOS X and BSDs, |dirfd| is a macro rather than a
  // function, so we need a little help to get it to work
#if defined(dirfd)
  { "OSFILE_SIZEOF_DIR", INT_TO_JSVAL(sizeof (DIR)) },

  { "OSFILE_OFFSETOF_DIR_DD_FD", INT_TO_JSVAL(offsetof (DIR, __dd_fd)) },
#endif

  // Defining |stat|

  { "OSFILE_SIZEOF_STAT", INT_TO_JSVAL(sizeof (struct stat)) },

  { "OSFILE_OFFSETOF_STAT_ST_MODE", INT_TO_JSVAL(offsetof (struct stat, st_mode)) },
  { "OSFILE_OFFSETOF_STAT_ST_UID", INT_TO_JSVAL(offsetof (struct stat, st_uid)) },
  { "OSFILE_OFFSETOF_STAT_ST_GID", INT_TO_JSVAL(offsetof (struct stat, st_gid)) },
  { "OSFILE_OFFSETOF_STAT_ST_SIZE", INT_TO_JSVAL(offsetof (struct stat, st_size)) },

#if defined(HAVE_ST_ATIMESPEC)
  { "OSFILE_OFFSETOF_STAT_ST_ATIME", INT_TO_JSVAL(offsetof (struct stat, st_atimespec)) },
  { "OSFILE_OFFSETOF_STAT_ST_MTIME", INT_TO_JSVAL(offsetof (struct stat, st_mtimespec)) },
  { "OSFILE_OFFSETOF_STAT_ST_CTIME", INT_TO_JSVAL(offsetof (struct stat, st_ctimespec)) },
#else
  { "OSFILE_OFFSETOF_STAT_ST_ATIME", INT_TO_JSVAL(offsetof (struct stat, st_atime)) },
  { "OSFILE_OFFSETOF_STAT_ST_MTIME", INT_TO_JSVAL(offsetof (struct stat, st_mtime)) },
  { "OSFILE_OFFSETOF_STAT_ST_CTIME", INT_TO_JSVAL(offsetof (struct stat, st_ctime)) },
#endif // defined(HAVE_ST_ATIME)

  // Several OSes have a birthtime field. For the moment, supporting only Darwin.
#if defined(_DARWIN_FEATURE_64_BIT_INODE)
  { "OSFILE_OFFSETOF_STAT_ST_BIRTHTIME", INT_TO_JSVAL(offsetof (struct stat, st_birthtime)) },
#endif // defined(_DARWIN_FEATURE_64_BIT_INODE)

  // Defining |statvfs|

  { "OSFILE_SIZEOF_STATVFS", INT_TO_JSVAL(sizeof (struct statvfs)) },

  { "OSFILE_OFFSETOF_STATVFS_F_BSIZE", INT_TO_JSVAL(offsetof (struct statvfs, f_bsize)) },
  { "OSFILE_OFFSETOF_STATVFS_F_BAVAIL", INT_TO_JSVAL(offsetof (struct statvfs, f_bavail)) },

#endif // defined(XP_UNIX)



  // System configuration

  // Under MacOSX, to avoid using deprecated functions that do not
  // match the constants we define in this object (including
  // |sizeof|/|offsetof| stuff, but not only), for a number of
  // functions, we need to adapt the name of the symbols we are using,
  // whenever macro _DARWIN_FEATURE_64_BIT_INODE is set. We export
  // this value to be able to do so from JavaScript.
#if defined(_DARWIN_FEATURE_64_BIT_INODE)
   { "_DARWIN_FEATURE_64_BIT_INODE", INT_TO_JSVAL(1) },
#endif // defined(_DARWIN_FEATURE_64_BIT_INODE)

  // Similar feature for Linux
#if defined(_STAT_VER)
  INT_CONSTANT(_STAT_VER),
#endif // defined(_STAT_VER)

  PROP_END
};


#if defined(XP_WIN)
/**
 * The properties defined in windows.h.
 *
 * If you extend this list of properties, please
 * separate categories ("errors", "open", etc.),
 * keep properties organized by alphabetical order
 * and #ifdef-away properties that are not portable.
 */
static const dom::ConstantSpec gWinProperties[] =
{
  // FormatMessage flags
  INT_CONSTANT(FORMAT_MESSAGE_FROM_SYSTEM),
  INT_CONSTANT(FORMAT_MESSAGE_IGNORE_INSERTS),

  // The max length of paths
  INT_CONSTANT(MAX_PATH),

  // CreateFile desired access
  INT_CONSTANT(GENERIC_ALL),
  INT_CONSTANT(GENERIC_EXECUTE),
  INT_CONSTANT(GENERIC_READ),
  INT_CONSTANT(GENERIC_WRITE),

  // CreateFile share mode
  INT_CONSTANT(FILE_SHARE_DELETE),
  INT_CONSTANT(FILE_SHARE_READ),
  INT_CONSTANT(FILE_SHARE_WRITE),

  // CreateFile creation disposition
  INT_CONSTANT(CREATE_ALWAYS),
  INT_CONSTANT(CREATE_NEW),
  INT_CONSTANT(OPEN_ALWAYS),
  INT_CONSTANT(OPEN_EXISTING),
  INT_CONSTANT(TRUNCATE_EXISTING),

  // CreateFile attributes
  INT_CONSTANT(FILE_ATTRIBUTE_ARCHIVE),
  INT_CONSTANT(FILE_ATTRIBUTE_DIRECTORY),
  INT_CONSTANT(FILE_ATTRIBUTE_HIDDEN),
  INT_CONSTANT(FILE_ATTRIBUTE_NORMAL),
  INT_CONSTANT(FILE_ATTRIBUTE_READONLY),
  INT_CONSTANT(FILE_ATTRIBUTE_REPARSE_POINT),
  INT_CONSTANT(FILE_ATTRIBUTE_SYSTEM),
  INT_CONSTANT(FILE_ATTRIBUTE_TEMPORARY),
  INT_CONSTANT(FILE_FLAG_BACKUP_SEMANTICS),

  // CreateFile error constant
  { "INVALID_HANDLE_VALUE", INT_TO_JSVAL(INT_PTR(INVALID_HANDLE_VALUE)) },


  // CreateFile flags
  INT_CONSTANT(FILE_FLAG_DELETE_ON_CLOSE),

  // SetFilePointer methods
  INT_CONSTANT(FILE_BEGIN),
  INT_CONSTANT(FILE_CURRENT),
  INT_CONSTANT(FILE_END),

  // SetFilePointer error constant
  UINT_CONSTANT(INVALID_SET_FILE_POINTER),

  // File attributes
  INT_CONSTANT(FILE_ATTRIBUTE_DIRECTORY),


  // MoveFile flags
  INT_CONSTANT(MOVEFILE_COPY_ALLOWED),
  INT_CONSTANT(MOVEFILE_REPLACE_EXISTING),

  // GetFileAttributes error constant
  INT_CONSTANT(INVALID_FILE_ATTRIBUTES),

  // GetNamedSecurityInfo and SetNamedSecurityInfo constants
  INT_CONSTANT(UNPROTECTED_DACL_SECURITY_INFORMATION),
  INT_CONSTANT(SE_FILE_OBJECT),
  INT_CONSTANT(DACL_SECURITY_INFORMATION),

  // Errors
  INT_CONSTANT(ERROR_INVALID_HANDLE),
  INT_CONSTANT(ERROR_ACCESS_DENIED),
  INT_CONSTANT(ERROR_DIR_NOT_EMPTY),
  INT_CONSTANT(ERROR_FILE_EXISTS),
  INT_CONSTANT(ERROR_ALREADY_EXISTS),
  INT_CONSTANT(ERROR_FILE_NOT_FOUND),
  INT_CONSTANT(ERROR_NO_MORE_FILES),
  INT_CONSTANT(ERROR_PATH_NOT_FOUND),
  INT_CONSTANT(ERROR_BAD_ARGUMENTS),
  INT_CONSTANT(ERROR_SHARING_VIOLATION),
  INT_CONSTANT(ERROR_NOT_SUPPORTED),

  PROP_END
};
#endif // defined(XP_WIN)


/**
 * Get a field of an object as an object.
 *
 * If the field does not exist, create it. If it exists but is not an
 * object, throw a JS error.
 */
JSObject *GetOrCreateObjectProperty(JSContext *cx, JS::Handle<JSObject*> aObject,
                                    const char *aProperty)
{
  JS::Rooted<JS::Value> val(cx);
  if (!JS_GetProperty(cx, aObject, aProperty, &val)) {
    return nullptr;
  }
  if (!val.isUndefined()) {
    if (val.isObject()) {
      return &val.toObject();
    }

    JS_ReportErrorNumber(cx, js::GetErrorMessage, nullptr,
      JSMSG_UNEXPECTED_TYPE, aProperty, "not an object");
    return nullptr;
  }
  return JS_DefineObject(cx, aObject, aProperty, nullptr, JSPROP_ENUMERATE);
}

/**
 * Set a property of an object from a nsString.
 *
 * If the nsString is void (i.e. IsVoid is true), do nothing.
 */
bool SetStringProperty(JSContext *cx, JS::Handle<JSObject*> aObject, const char *aProperty,
                       const nsString aValue)
{
  if (aValue.IsVoid()) {
    return true;
  }
  JSString* strValue = JS_NewUCStringCopyZ(cx, aValue.get());
  NS_ENSURE_TRUE(strValue, false);
  JS::Rooted<JS::Value> valValue(cx, STRING_TO_JSVAL(strValue));
  return JS_SetProperty(cx, aObject, aProperty, valValue);
}

/**
 * Define OS-specific constants.
 *
 * This function creates or uses JS object |OS.Constants| to store
 * all its constants.
 */
bool DefineOSFileConstants(JSContext *cx, JS::Handle<JSObject*> global)
{
  MOZ_ASSERT(gInitialized);

  if (gPaths == nullptr) {
    // If an initialization error was ignored, we may end up with
    // |gInitialized == true| but |gPaths == nullptr|. We cannot
    // |MOZ_ASSERT| this, as this would kill precompile_cache.js,
    // so we simply return an error.
    JS_ReportErrorNumber(cx, js::GetErrorMessage, nullptr,
      JSMSG_CANT_OPEN, "OSFileConstants", "initialization has failed");
    return false;
  }

  JS::Rooted<JSObject*> objOS(cx);
  if (!(objOS = GetOrCreateObjectProperty(cx, global, "OS"))) {
    return false;
  }
  JS::Rooted<JSObject*> objConstants(cx);
  if (!(objConstants = GetOrCreateObjectProperty(cx, objOS, "Constants"))) {
    return false;
  }

  // Build OS.Constants.libc

  JS::Rooted<JSObject*> objLibc(cx);
  if (!(objLibc = GetOrCreateObjectProperty(cx, objConstants, "libc"))) {
    return false;
  }
  if (!dom::DefineConstants(cx, objLibc, gLibcProperties)) {
    return false;
  }

#if defined(XP_WIN)
  // Build OS.Constants.Win

  JS::Rooted<JSObject*> objWin(cx);
  if (!(objWin = GetOrCreateObjectProperty(cx, objConstants, "Win"))) {
    return false;
  }
  if (!dom::DefineConstants(cx, objWin, gWinProperties)) {
    return false;
  }
#endif // defined(XP_WIN)

  // Build OS.Constants.Sys

  JS::Rooted<JSObject*> objSys(cx);
  if (!(objSys = GetOrCreateObjectProperty(cx, objConstants, "Sys"))) {
    return false;
  }

#if defined(MOZ_WIDGET_GONK)
    JSString* strVersion = JS_NewStringCopyZ(cx, "Gonk");
    if (!strVersion){
      return false;
    }
    JS::Rooted<JS::Value> valVersion(cx, STRING_TO_JSVAL(strVersion));
    if (!JS_SetProperty(cx, objSys, "Name", valVersion)) {
      return false;
  }
#else
  nsCOMPtr<nsIXULRuntime> runtime = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (runtime) {
    nsAutoCString os;
    DebugOnly<nsresult> rv = runtime->GetOS(os);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    JSString* strVersion = JS_NewStringCopyZ(cx, os.get());
    if (!strVersion) {
      return false;
    }

    JS::Rooted<JS::Value> valVersion(cx, STRING_TO_JSVAL(strVersion));
    if (!JS_SetProperty(cx, objSys, "Name", valVersion)) {
      return false;
    }
  }
#endif // defined(MOZ_WIDGET_GONK)

#if defined(DEBUG)
  JS::Rooted<JS::Value> valDebug(cx, JSVAL_TRUE);
  if (!JS_SetProperty(cx, objSys, "DEBUG", valDebug)) {
    return false;
  }
#endif

#if defined(HAVE_64BIT_BUILD)
  JS::Rooted<JS::Value> valBits(cx, INT_TO_JSVAL(64));
#else
  JS::Rooted<JS::Value> valBits(cx, INT_TO_JSVAL(32));
#endif //defined (HAVE_64BIT_BUILD)
  if (!JS_SetProperty(cx, objSys, "bits", valBits)) {
    return false;
  }

  dom::ConstantSpec umask_cs[] = {
    { "umask", UINT_TO_JSVAL(gUserUmask) },
    PROP_END
  };
  if (!dom::DefineConstants(cx, objSys, umask_cs)) {
      return false;
  }

  // Build OS.Constants.Path

  JS::Rooted<JSObject*> objPath(cx);
  if (!(objPath = GetOrCreateObjectProperty(cx, objConstants, "Path"))) {
    return false;
  }

  // Locate libxul
  // Note that we don't actually provide the full path, only the name of the
  // library, which is sufficient to link to the library using js-ctypes.

#if defined(XP_MACOSX)
  // Under MacOS X, for some reason, libxul is called simply "XUL",
  // and we need to provide the full path.
  nsAutoString libxul;
  libxul.Append(gPaths->libDir);
  libxul.AppendLiteral("/XUL");
#else
  // On other platforms, libxul is a library "xul" with regular
  // library prefix/suffix.
  nsAutoString libxul;
  libxul.AppendLiteral(DLL_PREFIX);
  libxul.AppendLiteral("xul");
  libxul.AppendLiteral(DLL_SUFFIX);
#endif // defined(XP_MACOSX)

  if (!SetStringProperty(cx, objPath, "libxul", libxul)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "libDir", gPaths->libDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "tmpDir", gPaths->tmpDir)) {
    return false;
  }

  // Configure profileDir only if it is available at this stage
  if (!gPaths->profileDir.IsVoid()
    && !SetStringProperty(cx, objPath, "profileDir", gPaths->profileDir)) {
    return false;
  }

  // Configure localProfileDir only if it is available at this stage
  if (!gPaths->localProfileDir.IsVoid()
    && !SetStringProperty(cx, objPath, "localProfileDir", gPaths->localProfileDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "homeDir", gPaths->homeDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "desktopDir", gPaths->desktopDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "userApplicationDataDir", gPaths->userApplicationDataDir)) {
    return false;
  }

#if defined(XP_WIN)
  if (!SetStringProperty(cx, objPath, "winAppDataDir", gPaths->winAppDataDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "winStartMenuProgsDir", gPaths->winStartMenuProgsDir)) {
    return false;
  }
#endif // defined(XP_WIN)

#if defined(XP_MACOSX)
  if (!SetStringProperty(cx, objPath, "macUserLibDir", gPaths->macUserLibDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "macLocalApplicationsDir", gPaths->macLocalApplicationsDir)) {
    return false;
  }

  if (!SetStringProperty(cx, objPath, "macTrashDir", gPaths->macTrashDir)) {
    return false;
  }
#endif // defined(XP_MACOSX)

  // sqlite3 is linked from different places depending on the platform
  nsAutoString libsqlite3;
#if defined(ANDROID)
  // On Android, we use the system's libsqlite3
  libsqlite3.AppendLiteral(DLL_PREFIX);
  libsqlite3.AppendLiteral("sqlite3");
  libsqlite3.AppendLiteral(DLL_SUFFIX);
#elif defined(XP_WIN)
  // On Windows, for some reason, this is part of nss3.dll
  libsqlite3.AppendLiteral(DLL_PREFIX);
  libsqlite3.AppendLiteral("nss3");
  libsqlite3.AppendLiteral(DLL_SUFFIX);
#else
    // On other platforms, we link sqlite3 into libxul
  libsqlite3 = libxul;
#endif // defined(ANDROID) || defined(XP_WIN)

  if (!SetStringProperty(cx, objPath, "libsqlite3", libsqlite3)) {
    return false;
  }

  return true;
}

NS_IMPL_ISUPPORTS(OSFileConstantsService, nsIOSFileConstantsService)

OSFileConstantsService::OSFileConstantsService()
{
  MOZ_ASSERT(NS_IsMainThread());
}

OSFileConstantsService::~OSFileConstantsService()
{
  mozilla::CleanupOSFileConstants();
}


NS_IMETHODIMP
OSFileConstantsService::Init(JSContext *aCx)
{
  nsresult rv = mozilla::InitOSFileConstants();
  if (NS_FAILED(rv)) {
    return rv;
  }

  mozJSComponentLoader* loader = mozJSComponentLoader::Get();
  JS::Rooted<JSObject*> targetObj(aCx);
  rv = loader->FindTargetObject(aCx, &targetObj);
  NS_ENSURE_SUCCESS(rv, rv);

  if (!mozilla::DefineOSFileConstants(aCx, targetObj)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

} // namespace mozilla
