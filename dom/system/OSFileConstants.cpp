/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "fcntl.h"
#include "errno.h"

#if defined(XP_UNIX)
#include "unistd.h"
#endif // defined(XP_UNIX)

#if defined(XP_MACOSX)
#include "copyfile.h"
#endif // defined(XP_MACOSX)

#if defined(XP_WIN)
#include <windows.h>
#endif // defined(XP_WIN)

#include "jsapi.h"
#include "jsfriendapi.h"
#include "BindingUtils.h"

// Used to provide information on the OS

#include "nsIXULRuntime.h"
#include "nsXPCOMCIDInternal.h"
#include "nsServiceManagerUtils.h"
#include "nsString.h"

#include "OSFileConstants.h"

/**
 * This module defines the basic libc constants (error numbers, open modes,
 * etc.) used by OS.File and possibly other OS-bound JavaScript libraries.
 */

namespace mozilla {

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
 * End marker for ConstantSpec
 */
#define PROP_END { NULL, JSVAL_VOID }


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
static dom::ConstantSpec gLibcProperties[] =
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

  // copyfile
#if defined(COPYFILE_DATA)
  INT_CONSTANT(COPYFILE_DATA),
  INT_CONSTANT(COPYFILE_EXCL),
  INT_CONSTANT(COPYFILE_XATTR),
  INT_CONSTANT(COPYFILE_STAT),
  INT_CONSTANT(COPYFILE_ACL),
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

  // Constants used to define data structures
  //
  // Many data structures have different fields/sizes/etc. on
  // various OSes / versions of the same OS / platforms. For these
  // data structures, we need to compute and export from C the size
  // and, if necessary, the offset of fields, so as to be able to
  // define the structure in JS.

#if defined(XP_UNIX)
  // The size of |mode_t|.
  {"OSFILE_SIZEOF_MODE_T", INT_TO_JSVAL(sizeof (mode_t)) },
#endif // defined(XP_UNIX)

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
static dom::ConstantSpec gWinProperties[] =
{
  // FormatMessage flags
  INT_CONSTANT(FORMAT_MESSAGE_FROM_SYSTEM),
  INT_CONSTANT(FORMAT_MESSAGE_IGNORE_INSERTS),

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
  INT_CONSTANT(FILE_ATTRIBUTE_NORMAL),
  INT_CONSTANT(FILE_ATTRIBUTE_READONLY),
  INT_CONSTANT(FILE_ATTRIBUTE_TEMPORARY),

  // CreateFile error constant
  { "INVALID_HANDLE_VALUE", INT_TO_JSVAL(INT_PTR(INVALID_HANDLE_VALUE)) },


  // CreateFile flags
  INT_CONSTANT(FILE_FLAG_DELETE_ON_CLOSE),

  // SetFilePointer methods
  INT_CONSTANT(FILE_BEGIN),
  INT_CONSTANT(FILE_CURRENT),
  INT_CONSTANT(FILE_END),

  // SetFilePointer error constant
  INT_CONSTANT(INVALID_SET_FILE_POINTER),

  // MoveFile flags
  INT_CONSTANT(MOVEFILE_COPY_ALLOWED),
  INT_CONSTANT(MOVEFILE_REPLACE_EXISTING),

  // Errors
  INT_CONSTANT(ERROR_FILE_EXISTS),
  INT_CONSTANT(ERROR_FILE_NOT_FOUND),
  INT_CONSTANT(ERROR_ACCESS_DENIED),

  PROP_END
};
#endif // defined(XP_WIN)


/**
 * Get a field of an object as an object.
 *
 * If the field does not exist, create it. If it exists but is not an
 * object, throw a JS error.
 */
JSObject *GetOrCreateObjectProperty(JSContext *cx, JSObject *aObject,
                                    const char *aProperty)
{
  JS::Value val;
  if (!JS_GetProperty(cx, aObject, aProperty, &val)) {
    return NULL;
  }
  if (!val.isUndefined()) {
    if (val.isObject()) {
      return &val.toObject();
    }

    JS_ReportErrorNumber(cx, js_GetErrorMessage, NULL,
      JSMSG_UNEXPECTED_TYPE, aProperty, "not an object");
    return NULL;
  }
  return JS_DefineObject(cx, aObject, aProperty, NULL, NULL, JSPROP_ENUMERATE);
}

/**
 * Define OS-specific constants.
 *
 * This function creates or uses JS object |OS.Constants| to store
 * all its constants.
 */
bool DefineOSFileConstants(JSContext *cx, JSObject *global)
{
  JSObject *objOS;
  if (!(objOS = GetOrCreateObjectProperty(cx, global, "OS"))) {
    return false;
  }
  JSObject *objConstants;
  if (!(objConstants = GetOrCreateObjectProperty(cx, objOS, "Constants"))) {
    return false;
  }
  JSObject *objLibc;
  if (!(objLibc = GetOrCreateObjectProperty(cx, objConstants, "libc"))) {
    return false;
  }
  if (!dom::DefineConstants(cx, objLibc, gLibcProperties)) {
    return false;
  }
#if defined(XP_WIN)
  JSObject *objWin;
  if (!(objWin = GetOrCreateObjectProperty(cx, objConstants, "Win"))) {
    return false;
  }
  if (!dom::DefineConstants(cx, objWin, gWinProperties)) {
    return false;
  }
#endif // defined(XP_WIN)
  JSObject *objSys;
  if (!(objSys = GetOrCreateObjectProperty(cx, objConstants, "Sys"))) {
    return false;
  }

  nsCOMPtr<nsIXULRuntime> runtime = do_GetService(XULRUNTIME_SERVICE_CONTRACTID);
  if (runtime) {
    nsCAutoString os;
    nsresult rv = runtime->GetOS(os);
    MOZ_ASSERT(NS_SUCCEEDED(rv));

    JSString* strVersion = JS_NewStringCopyZ(cx, os.get());
    if (!strVersion) {
      return false;
    }

    jsval valVersion = STRING_TO_JSVAL(strVersion);
    if (!JS_SetProperty(cx, objSys, "Name", &valVersion)) {
      return false;
    }
  }

  return true;
}

} // namespace mozilla

