/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// OSObject.h - os object for exposing posix system calls in the JS shell

#include "shell/OSObject.h"

#include "mozilla/TextUtils.h"

#include <errno.h>
#include <stdlib.h>
#ifdef XP_WIN
#  include <direct.h>
#  include <process.h>
#  include <string.h>
#else
#  include <sys/wait.h>
#  include <unistd.h>
#endif

#include "jsapi.h"
// For JSFunctionSpecWithHelp
#include "jsfriendapi.h"

#include "gc/FreeOp.h"
#include "js/CharacterEncoding.h"
#include "js/Conversions.h"
#include "js/PropertySpec.h"
#include "js/Wrapper.h"
#include "shell/jsshell.h"
#include "util/StringBuffer.h"
#include "util/Text.h"
#include "util/Windows.h"
#include "vm/JSObject.h"
#include "vm/TypedArrayObject.h"

#include "vm/JSObject-inl.h"

#ifdef XP_WIN
#  ifndef PATH_MAX
#    define PATH_MAX (MAX_PATH > _MAX_DIR ? MAX_PATH : _MAX_DIR)
#  endif
#  define getcwd _getcwd
#else
#  include <libgen.h>
#endif

using js::shell::RCFile;

namespace js {
namespace shell {

#ifdef XP_WIN
const char PathSeparator = '\\';
#else
const char PathSeparator = '/';
#endif

static bool IsAbsolutePath(const UniqueChars& filename) {
  const char* pathname = filename.get();

  if (pathname[0] == PathSeparator) {
    return true;
  }

#ifdef XP_WIN
  // On Windows there are various forms of absolute paths (see
  // http://msdn.microsoft.com/en-us/library/windows/desktop/aa365247%28v=vs.85%29.aspx
  // for details):
  //
  //   "\..."
  //   "\\..."
  //   "C:\..."
  //
  // The first two cases are handled by the test above so we only need a test
  // for the last one here.

  if ((strlen(pathname) > 3 && mozilla::IsAsciiAlpha(pathname[0]) &&
       pathname[1] == ':' && pathname[2] == '\\')) {
    return true;
  }
#endif

  return false;
}

/*
 * Resolve a (possibly) relative filename to an absolute path. If
 * |scriptRelative| is true, then the result will be relative to the directory
 * containing the currently-running script, or the current working directory if
 * the currently-running script is "-e" (namely, you're using it from the
 * command line.) Otherwise, it will be relative to the current working
 * directory.
 */
JSString* ResolvePath(JSContext* cx, HandleString filenameStr,
                      PathResolutionMode resolveMode) {
  if (!filenameStr) {
#ifdef XP_WIN
    return JS_NewStringCopyZ(cx, "nul");
#else
    return JS_NewStringCopyZ(cx, "/dev/null");
#endif
  }

  UniqueChars filename = JS_EncodeStringToLatin1(cx, filenameStr);
  if (!filename) {
    return nullptr;
  }

  if (IsAbsolutePath(filename)) {
    return filenameStr;
  }

  JS::AutoFilename scriptFilename;
  if (resolveMode == ScriptRelative) {
    // Get the currently executing script's name.
    if (!DescribeScriptedCaller(cx, &scriptFilename)) {
      return nullptr;
    }

    if (!scriptFilename.get()) {
      return nullptr;
    }

    if (strcmp(scriptFilename.get(), "-e") == 0 ||
        strcmp(scriptFilename.get(), "typein") == 0) {
      resolveMode = RootRelative;
    }
  }

  char buffer[PATH_MAX + 1];
  if (resolveMode == ScriptRelative) {
#ifdef XP_WIN
    // The docs say it can return EINVAL, but the compiler says it's void
    _splitpath(scriptFilename.get(), nullptr, buffer, nullptr, nullptr);
#else
    strncpy(buffer, scriptFilename.get(), PATH_MAX + 1);
    if (buffer[PATH_MAX] != '\0') {
      return nullptr;
    }

    // dirname(buffer) might return buffer, or it might return a
    // statically-allocated string
    memmove(buffer, dirname(buffer), strlen(buffer) + 1);
#endif
  } else {
    const char* cwd = getcwd(buffer, PATH_MAX);
    if (!cwd) {
      return nullptr;
    }
  }

  size_t len = strlen(buffer);
  buffer[len] = '/';
  strncpy(buffer + len + 1, filename.get(), sizeof(buffer) - (len + 1));
  if (buffer[PATH_MAX] != '\0') {
    return nullptr;
  }

  return JS_NewStringCopyZ(cx, buffer);
}

JSObject* FileAsTypedArray(JSContext* cx, JS::HandleString pathnameStr) {
  UniqueChars pathname = JS_EncodeStringToLatin1(cx, pathnameStr);
  if (!pathname) {
    return nullptr;
  }

  FILE* file = fopen(pathname.get(), "rb");
  if (!file) {
    /*
     * Use Latin1 variant here because the encoding of the return value of
     * strerror function can be non-UTF-8.
     */
    JS_ReportErrorLatin1(cx, "can't open %s: %s", pathname.get(),
                         strerror(errno));
    return nullptr;
  }
  AutoCloseFile autoClose(file);

  RootedObject obj(cx);
  if (fseek(file, 0, SEEK_END) != 0) {
    pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
    if (!pathname) {
      return nullptr;
    }
    JS_ReportErrorUTF8(cx, "can't seek end of %s", pathname.get());
  } else {
    size_t len = ftell(file);
    if (fseek(file, 0, SEEK_SET) != 0) {
      pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
      if (!pathname) {
        return nullptr;
      }
      JS_ReportErrorUTF8(cx, "can't seek start of %s", pathname.get());
    } else {
      if (len > ArrayBufferObject::MaxBufferByteLength) {
        JS_ReportErrorUTF8(cx, "file %s is too large for a Uint8Array",
                           pathname.get());
        return nullptr;
      }
      obj = JS_NewUint8Array(cx, len);
      if (!obj) {
        return nullptr;
      }
      js::TypedArrayObject& ta = obj->as<js::TypedArrayObject>();
      if (ta.isSharedMemory()) {
        // Must opt in to use shared memory.  For now, don't.
        //
        // (It is incorrect to read into the buffer without
        // synchronization since that can create a race.  A
        // lock here won't fix it - both sides must
        // participate.  So what one must do is to create a
        // temporary buffer, read into that, and use a
        // race-safe primitive to copy memory into the
        // buffer.)
        pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
        if (!pathname) {
          return nullptr;
        }
        JS_ReportErrorUTF8(cx, "can't read %s: shared memory buffer",
                           pathname.get());
        return nullptr;
      }
      char* buf = static_cast<char*>(ta.dataPointerUnshared());
      size_t cc = fread(buf, 1, len, file);
      if (cc != len) {
        if (ptrdiff_t(cc) < 0) {
          /*
           * Use Latin1 variant here because the encoding of the return
           * value of strerror function can be non-UTF-8.
           */
          JS_ReportErrorLatin1(cx, "can't read %s: %s", pathname.get(),
                               strerror(errno));
        } else {
          pathname = JS_EncodeStringToUTF8(cx, pathnameStr);
          if (!pathname) {
            return nullptr;
          }
          JS_ReportErrorUTF8(cx, "can't read %s: short read", pathname.get());
        }
        obj = nullptr;
      }
    }
  }
  return obj;
}

/**
 * Return the current working directory or |null| on failure.
 */
UniqueChars GetCWD() {
  char buffer[PATH_MAX + 1];
  const char* cwd = getcwd(buffer, PATH_MAX);
  if (!cwd) {
    return UniqueChars();
  }
  return js::DuplicateString(buffer);
}

static bool ReadFile(JSContext* cx, unsigned argc, Value* vp,
                     bool scriptRelative) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() < 1 || args.length() > 2) {
    JS_ReportErrorNumberASCII(
        cx, js::shell::my_GetErrorMessage, nullptr,
        args.length() < 1 ? JSSMSG_NOT_ENOUGH_ARGS : JSSMSG_TOO_MANY_ARGS,
        "snarf");
    return false;
  }

  if (!args[0].isString() || (args.length() == 2 && !args[1].isString())) {
    JS_ReportErrorNumberASCII(cx, js::shell::my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "snarf");
    return false;
  }

  RootedString givenPath(cx, args[0].toString());
  RootedString str(
      cx, js::shell::ResolvePath(
              cx, givenPath, scriptRelative ? ScriptRelative : RootRelative));
  if (!str) {
    return false;
  }

  if (args.length() > 1) {
    JSString* opt = JS::ToString(cx, args[1]);
    if (!opt) {
      return false;
    }
    bool match;
    if (!JS_StringEqualsAscii(cx, opt, "binary", &match)) {
      return false;
    }
    if (match) {
      JSObject* obj;
      if (!(obj = FileAsTypedArray(cx, str))) {
        return false;
      }
      args.rval().setObject(*obj);
      return true;
    }
  }

  if (!(str = FileAsString(cx, str))) {
    return false;
  }
  args.rval().setString(str);
  return true;
}

static bool osfile_readFile(JSContext* cx, unsigned argc, Value* vp) {
  return ReadFile(cx, argc, vp, false);
}

static bool osfile_readRelativeToScript(JSContext* cx, unsigned argc,
                                        Value* vp) {
  return ReadFile(cx, argc, vp, true);
}

static bool osfile_writeTypedArrayToFile(JSContext* cx, unsigned argc,
                                         Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 2 || !args[0].isString() || !args[1].isObject() ||
      !args[1].toObject().is<TypedArrayObject>()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "writeTypedArrayToFile");
    return false;
  }

  RootedString givenPath(cx, args[0].toString());
  RootedString str(cx, ResolvePath(cx, givenPath, RootRelative));
  if (!str) {
    return false;
  }

  UniqueChars filename = JS_EncodeStringToLatin1(cx, str);
  if (!filename) {
    return false;
  }

  FILE* file = fopen(filename.get(), "wb");
  if (!file) {
    /*
     * Use Latin1 variant here because the encoding of the return value of
     * strerror function can be non-UTF-8.
     */
    JS_ReportErrorLatin1(cx, "can't open %s: %s", filename.get(),
                         strerror(errno));
    return false;
  }
  AutoCloseFile autoClose(file);

  TypedArrayObject* obj = &args[1].toObject().as<TypedArrayObject>();

  if (obj->isSharedMemory()) {
    // Must opt in to use shared memory.  For now, don't.
    //
    // See further comments in FileAsTypedArray, above.
    filename = JS_EncodeStringToUTF8(cx, str);
    if (!filename) {
      return false;
    }
    JS_ReportErrorUTF8(cx, "can't write %s: shared memory buffer",
                       filename.get());
    return false;
  }
  void* buf = obj->dataPointerUnshared();
  if (fwrite(buf, obj->bytesPerElement(), obj->length(), file) !=
          obj->length() ||
      !autoClose.release()) {
    filename = JS_EncodeStringToUTF8(cx, str);
    if (!filename) {
      return false;
    }
    JS_ReportErrorUTF8(cx, "can't write %s", filename.get());
    return false;
  }

  args.rval().setUndefined();
  return true;
}

/* static */
RCFile* RCFile::create(JSContext* cx, const char* filename, const char* mode) {
  FILE* fp = fopen(filename, mode);
  if (!fp) {
    return nullptr;
  }

  RCFile* file = cx->new_<RCFile>(fp);
  if (!file) {
    fclose(fp);
    return nullptr;
  }

  return file;
}

void RCFile::close() {
  if (fp) {
    fclose(fp);
  }
  fp = nullptr;
}

bool RCFile::release() {
  if (--numRefs) {
    return false;
  }
  this->close();
  return true;
}

class FileObject : public NativeObject {
  enum : uint32_t { FILE_SLOT = 0, NUM_SLOTS };

 public:
  static const JSClass class_;

  static FileObject* create(JSContext* cx, RCFile* file) {
    FileObject* obj = js::NewBuiltinClassInstance<FileObject>(cx);
    if (!obj) {
      return nullptr;
    }

    InitReservedSlot(obj, FILE_SLOT, file, MemoryUse::FileObjectFile);
    file->acquire();
    return obj;
  }

  static void finalize(JSFreeOp* fop, JSObject* obj) {
    FileObject* fileObj = &obj->as<FileObject>();
    RCFile* file = fileObj->rcFile();
    fop->removeCellMemory(obj, sizeof(*file), MemoryUse::FileObjectFile);
    if (file->release()) {
      fop->deleteUntracked(file);
    }
  }

  bool isOpen() {
    RCFile* file = rcFile();
    return file && file->isOpen();
  }

  void close() {
    if (!isOpen()) {
      return;
    }
    rcFile()->close();
  }

  RCFile* rcFile() {
    return reinterpret_cast<RCFile*>(
        js::GetReservedSlot(this, FILE_SLOT).toPrivate());
  }
};

static const JSClassOps FileObjectClassOps = {
    nullptr,              /* addProperty */
    nullptr,              /* delProperty */
    nullptr,              /* enumerate */
    nullptr,              /* newEnumerate */
    nullptr,              /* resolve */
    nullptr,              /* mayResolve */
    FileObject::finalize, /* finalize */
    nullptr,              /* call */
    nullptr,              /* hasInstance */
    nullptr,              /* construct */
    nullptr               /* trace */
};

const JSClass FileObject::class_ = {
    "File",
    JSCLASS_HAS_RESERVED_SLOTS(FileObject::NUM_SLOTS) |
        JSCLASS_FOREGROUND_FINALIZE,
    &FileObjectClassOps};

static FileObject* redirect(JSContext* cx, HandleString relFilename,
                            RCFile** globalFile) {
  RootedString filename(cx, ResolvePath(cx, relFilename, RootRelative));
  if (!filename) {
    return nullptr;
  }
  UniqueChars filenameABS = JS_EncodeStringToLatin1(cx, filename);
  if (!filenameABS) {
    return nullptr;
  }
  RCFile* file = RCFile::create(cx, filenameABS.get(), "wb");
  if (!file) {
    /*
     * Use Latin1 variant here because the encoding of the return value of
     * strerror function can be non-UTF-8.
     */
    JS_ReportErrorLatin1(cx, "cannot redirect to %s: %s", filenameABS.get(),
                         strerror(errno));
    return nullptr;
  }

  // Grant the global gOutFile ownership of the new file, release ownership
  // of its old file, and return a FileObject owning the old file.
  file->acquire();  // Global owner of new file

  FileObject* fileObj =
      FileObject::create(cx, *globalFile);  // Newly created owner of old file
  if (!fileObj) {
    file->release();
    return nullptr;
  }

  (*globalFile)->release();  // Release (global) ownership of old file.
  *globalFile = file;

  return fileObj;
}

static bool Redirect(JSContext* cx, const CallArgs& args, RCFile** outFile) {
  if (args.length() > 1) {
    JS_ReportErrorNumberASCII(cx, js::shell::my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "redirect");
    return false;
  }

  RCFile* oldFile = *outFile;
  RootedObject oldFileObj(cx, FileObject::create(cx, oldFile));
  if (!oldFileObj) {
    return false;
  }

  if (args.get(0).isUndefined()) {
    args.rval().setObject(*oldFileObj);
    return true;
  }

  if (args[0].isObject()) {
    Rooted<FileObject*> fileObj(cx,
                                args[0].toObject().maybeUnwrapIf<FileObject>());
    if (!fileObj) {
      JS_ReportErrorNumberASCII(cx, js::shell::my_GetErrorMessage, nullptr,
                                JSSMSG_INVALID_ARGS, "redirect");
      return false;
    }

    // Passed in a FileObject. Create a FileObject for the previous
    // global file, and set the global file to the passed-in one.
    *outFile = fileObj->rcFile();
    (*outFile)->acquire();
    oldFile->release();

    args.rval().setObject(*oldFileObj);
    return true;
  }

  RootedString filename(cx);
  if (!args[0].isNull()) {
    filename = JS::ToString(cx, args[0]);
    if (!filename) {
      return false;
    }
  }

  if (!redirect(cx, filename, outFile)) {
    return false;
  }

  args.rval().setObject(*oldFileObj);
  return true;
}

static bool osfile_redirectOutput(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  ShellContext* scx = GetShellContext(cx);
  return Redirect(cx, args, scx->outFilePtr);
}

static bool osfile_redirectError(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  ShellContext* scx = GetShellContext(cx);
  return Redirect(cx, args, scx->errFilePtr);
}

static bool osfile_close(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  Rooted<FileObject*> fileObj(cx);
  if (args.get(0).isObject()) {
    fileObj = args[0].toObject().maybeUnwrapIf<FileObject>();
  }

  if (!fileObj) {
    JS_ReportErrorNumberASCII(cx, js::shell::my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "close");
    return false;
  }

  fileObj->close();

  args.rval().setUndefined();
  return true;
}

// clang-format off
static const JSFunctionSpecWithHelp osfile_functions[] = {
    JS_FN_HELP("readFile", osfile_readFile, 1, 0,
"readFile(filename, [\"binary\"])",
"  Read entire contents of filename. Returns a string, unless \"binary\" is passed\n"
"  as the second argument, in which case it returns a Uint8Array. Filename is\n"
"  relative to the current working directory."),

    JS_FN_HELP("readRelativeToScript", osfile_readRelativeToScript, 1, 0,
"readRelativeToScript(filename, [\"binary\"])",
"  Read filename into returned string. Filename is relative to the directory\n"
"  containing the current script."),

    JS_FS_HELP_END
};
// clang-format on

// clang-format off
static const JSFunctionSpecWithHelp osfile_unsafe_functions[] = {
    JS_FN_HELP("writeTypedArrayToFile", osfile_writeTypedArrayToFile, 2, 0,
"writeTypedArrayToFile(filename, data)",
"  Write the contents of a typed array to the named file."),

    JS_FN_HELP("redirect", osfile_redirectOutput, 1, 0,
"redirect([path-or-object])",
"  Redirect print() output to the named file.\n"
"   Return an opaque object representing the previous destination, which\n"
"   may be passed into redirect() later to restore the output."),

    JS_FN_HELP("redirectErr", osfile_redirectError, 1, 0,
"redirectErr([path-or-object])",
"  Same as redirect(), but for printErr"),

    JS_FN_HELP("close", osfile_close, 1, 0,
"close(object)",
"  Close the file returned by an earlier redirect call."),

    JS_FS_HELP_END
};
// clang-format on

static bool ospath_isAbsolute(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() != 1 || !args[0].isString()) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "isAbsolute");
    return false;
  }

  UniqueChars path = JS_EncodeStringToLatin1(cx, args[0].toString());
  if (!path) {
    return false;
  }

  args.rval().setBoolean(IsAbsolutePath(path));
  return true;
}

static bool ospath_join(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() < 1) {
    JS_ReportErrorNumberASCII(cx, my_GetErrorMessage, nullptr,
                              JSSMSG_INVALID_ARGS, "join");
    return false;
  }

  // This function doesn't take into account some aspects of Windows paths,
  // e.g. the drive letter is always reset when an absolute path is appended.

  JSStringBuilder buffer(cx);

  for (unsigned i = 0; i < args.length(); i++) {
    if (!args[i].isString()) {
      JS_ReportErrorASCII(cx, "join expects string arguments only");
      return false;
    }

    UniqueChars path = JS_EncodeStringToLatin1(cx, args[i].toString());
    if (!path) {
      return false;
    }

    if (IsAbsolutePath(path)) {
      MOZ_ALWAYS_TRUE(buffer.resize(0));
    } else if (i != 0) {
      if (!buffer.append(PathSeparator)) {
        return false;
      }
    }

    if (!buffer.append(args[i].toString())) {
      return false;
    }
  }

  JSString* result = buffer.finishString();
  if (!result) {
    return false;
  }

  args.rval().setString(result);
  return true;
}

// clang-format off
static const JSFunctionSpecWithHelp ospath_functions[] = {
    JS_FN_HELP("isAbsolute", ospath_isAbsolute, 1, 0,
"isAbsolute(path)",
"  Return whether the given path is absolute."),

    JS_FN_HELP("join", ospath_join, 1, 0,
"join(paths...)",
"  Join one or more path components in a platform independent way."),

    JS_FS_HELP_END
};
// clang-format on

static bool os_getenv(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() < 1) {
    JS_ReportErrorASCII(cx, "os.getenv requires 1 argument");
    return false;
  }
  RootedString key(cx, ToString(cx, args[0]));
  if (!key) {
    return false;
  }
  UniqueChars keyBytes = JS_EncodeStringToUTF8(cx, key);
  if (!keyBytes) {
    return false;
  }

  if (const char* valueBytes = getenv(keyBytes.get())) {
    RootedString value(cx, JS_NewStringCopyZ(cx, valueBytes));
    if (!value) {
      return false;
    }
    args.rval().setString(value);
  } else {
    args.rval().setUndefined();
  }
  return true;
}

static bool os_getpid(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  if (args.length() != 0) {
    JS_ReportErrorASCII(cx, "os.getpid takes no arguments");
    return false;
  }
  args.rval().setInt32(getpid());
  return true;
}

#if !defined(XP_WIN)

// There are two possible definitions of strerror_r floating around. The GNU
// one returns a char* which may or may not be the buffer you passed in. The
// other one returns an integer status code, and always writes the result into
// the provided buffer.

inline char* strerror_message(int result, char* buffer) {
  return result == 0 ? buffer : nullptr;
}

inline char* strerror_message(char* result, char* buffer) { return result; }

#endif

static void ReportSysError(JSContext* cx, const char* prefix) {
  char buffer[200];

#if defined(XP_WIN)
  strerror_s(buffer, sizeof(buffer), errno);
  const char* errstr = buffer;
#else
  const char* errstr =
      strerror_message(strerror_r(errno, buffer, sizeof(buffer)), buffer);
#endif

  if (!errstr) {
    errstr = "unknown error";
  }

  /*
   * Use Latin1 variant here because the encoding of the return value of
   * strerror_s and strerror_r function can be non-UTF-8.
   */
  JS_ReportErrorLatin1(cx, "%s: %s", prefix, errstr);
}

static bool os_system(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() == 0) {
    JS_ReportErrorASCII(cx, "os.system requires 1 argument");
    return false;
  }

  JSString* str = JS::ToString(cx, args[0]);
  if (!str) {
    return false;
  }

  UniqueChars command = JS_EncodeStringToLatin1(cx, str);
  if (!command) {
    return false;
  }

  int result = system(command.get());
  if (result == -1) {
    ReportSysError(cx, "system call failed");
    return false;
  }

  args.rval().setInt32(result);
  return true;
}

#ifndef XP_WIN
static bool os_spawn(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  if (args.length() == 0) {
    JS_ReportErrorASCII(cx, "os.spawn requires 1 argument");
    return false;
  }

  JSString* str = JS::ToString(cx, args[0]);
  if (!str) {
    return false;
  }

  UniqueChars command = JS_EncodeStringToLatin1(cx, str);
  if (!command) {
    return false;
  }

  int32_t childPid = fork();
  if (childPid == -1) {
    ReportSysError(cx, "fork failed");
    return false;
  }

  if (childPid) {
    args.rval().setInt32(childPid);
    return true;
  }

  // We are in the child

  const char* cmd[] = {"sh", "-c", nullptr, nullptr};
  cmd[2] = command.get();

  execvp("sh", (char* const*)cmd);
  exit(1);
}

static bool os_kill(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);
  int32_t pid;
  if (args.length() < 1) {
    JS_ReportErrorASCII(cx, "os.kill requires 1 argument");
    return false;
  }
  if (!JS::ToInt32(cx, args[0], &pid)) {
    return false;
  }

  // It is too easy to kill yourself accidentally with os.kill("goose").
  if (pid == 0 && !args[0].isInt32()) {
    JS_ReportErrorASCII(cx, "os.kill requires numeric pid");
    return false;
  }

  int signal = SIGINT;
  if (args.length() > 1) {
    if (!JS::ToInt32(cx, args[1], &signal)) {
      return false;
    }
  }

  int status = kill(pid, signal);
  if (status == -1) {
    ReportSysError(cx, "kill failed");
    return false;
  }

  args.rval().setUndefined();
  return true;
}

static bool os_waitpid(JSContext* cx, unsigned argc, Value* vp) {
  CallArgs args = CallArgsFromVp(argc, vp);

  int32_t pid;
  if (args.length() == 0) {
    pid = -1;
  } else {
    if (!JS::ToInt32(cx, args[0], &pid)) {
      return false;
    }
  }

  bool nohang = false;
  if (args.length() >= 2) {
    nohang = JS::ToBoolean(args[1]);
  }

  int status = 0;
  pid_t result = waitpid(pid, &status, nohang ? WNOHANG : 0);
  if (result == -1) {
    ReportSysError(cx, "os.waitpid failed");
    return false;
  }

  RootedObject info(cx, JS_NewPlainObject(cx));
  if (!info) {
    return false;
  }

  RootedValue v(cx);
  if (result != 0) {
    v.setInt32(result);
    if (!JS_DefineProperty(cx, info, "pid", v, JSPROP_ENUMERATE)) {
      return false;
    }
    if (WIFEXITED(status)) {
      v.setInt32(WEXITSTATUS(status));
      if (!JS_DefineProperty(cx, info, "exitStatus", v, JSPROP_ENUMERATE)) {
        return false;
      }
    }
  }

  args.rval().setObject(*info);
  return true;
}
#endif

// clang-format off
static const JSFunctionSpecWithHelp os_functions[] = {
    JS_FN_HELP("getenv", os_getenv, 1, 0,
"getenv(variable)",
"  Get the value of an environment variable."),

    JS_FN_HELP("getpid", os_getpid, 0, 0,
"getpid()",
"  Return the current process id."),

    JS_FN_HELP("system", os_system, 1, 0,
"system(command)",
"  Execute command on the current host, returning result code or throwing an\n"
"  exception on failure."),

#ifndef XP_WIN
    JS_FN_HELP("spawn", os_spawn, 1, 0,
"spawn(command)",
"  Start up a separate process running the given command. Returns the pid."),

    JS_FN_HELP("kill", os_kill, 1, 0,
"kill(pid[, signal])",
"  Send a signal to the given pid. The default signal is SIGINT. The signal\n"
"  passed in must be numeric, if given."),

    JS_FN_HELP("waitpid", os_waitpid, 1, 0,
"waitpid(pid[, nohang])",
"  Calls waitpid(). 'nohang' is a boolean indicating whether to pass WNOHANG.\n"
"  The return value is an object containing a 'pid' field, if a process was waitable\n"
"  and an 'exitStatus' field if a pid exited."),
#endif

    JS_FS_HELP_END
};
// clang-format on

bool DefineOS(JSContext* cx, HandleObject global, bool fuzzingSafe,
              RCFile** shellOut, RCFile** shellErr) {
  RootedObject obj(cx, JS_NewPlainObject(cx));
  if (!obj || !JS_DefineProperty(cx, global, "os", obj, 0)) {
    return false;
  }

  if (!fuzzingSafe) {
    if (!JS_DefineFunctionsWithHelp(cx, obj, os_functions)) {
      return false;
    }
  }

  RootedObject osfile(cx, JS_NewPlainObject(cx));
  if (!osfile || !JS_DefineFunctionsWithHelp(cx, osfile, osfile_functions) ||
      !JS_DefineProperty(cx, obj, "file", osfile, 0)) {
    return false;
  }

  if (!fuzzingSafe) {
    if (!JS_DefineFunctionsWithHelp(cx, osfile, osfile_unsafe_functions)) {
      return false;
    }
  }

  if (!GenerateInterfaceHelp(cx, osfile, "os.file")) {
    return false;
  }

  RootedObject ospath(cx, JS_NewPlainObject(cx));
  if (!ospath || !JS_DefineFunctionsWithHelp(cx, ospath, ospath_functions) ||
      !JS_DefineProperty(cx, obj, "path", ospath, 0) ||
      !GenerateInterfaceHelp(cx, ospath, "os.path")) {
    return false;
  }

  if (!GenerateInterfaceHelp(cx, obj, "os")) {
    return false;
  }

  ShellContext* scx = GetShellContext(cx);
  scx->outFilePtr = shellOut;
  scx->errFilePtr = shellErr;

  // For backwards compatibility, expose various os.file.* functions as
  // direct methods on the global.
  struct Export {
    const char* src;
    const char* dst;
  };

  const Export osfile_exports[] = {
      {"readFile", "read"},
      {"readFile", "snarf"},
      {"readRelativeToScript", "readRelativeToScript"},
  };

  for (auto pair : osfile_exports) {
    if (!CreateAlias(cx, pair.dst, osfile, pair.src)) {
      return false;
    }
  }

  if (!fuzzingSafe) {
    const Export unsafe_osfile_exports[] = {{"redirect", "redirect"},
                                            {"redirectErr", "redirectErr"}};

    for (auto pair : unsafe_osfile_exports) {
      if (!CreateAlias(cx, pair.dst, osfile, pair.src)) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace shell
}  // namespace js
