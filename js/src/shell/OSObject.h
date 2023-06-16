/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// OSObject.h - os object for exposing posix system calls in the JS shell

#ifndef shell_OSObject_h
#define shell_OSObject_h

#include <stdio.h>

#include "js/TypeDecls.h"
#include "js/Utility.h"

class JSLinearString;

namespace js {
namespace shell {

#ifdef XP_WIN
constexpr char PathSeparator = '\\';
#else
constexpr char PathSeparator = '/';
#endif

struct RCFile;

/* Define an os object on the given global object. */
bool DefineOS(JSContext* cx, JS::HandleObject global, bool fuzzingSafe,
              RCFile** shellOut, RCFile** shellErr);

enum PathResolutionMode { RootRelative, ScriptRelative };

bool IsAbsolutePath(JSLinearString* filename);

JSString* ResolvePath(JSContext* cx, JS::HandleString filenameStr,
                      PathResolutionMode resolveMode);

JSObject* FileAsTypedArray(JSContext* cx, JS::HandleString pathnameStr);

/**
 * Return the current working directory as a UTF-8 encoded string.
 *
 * @param cx current js-context
 * @return the working directory name or {@code nullptr} on error
 */
JS::UniqueChars GetCWD(JSContext* cx);

/**
 * Open the requested file.
 *
 * @param cx current js-context
 * @param filename file name encoded in UTF-8
 * @param mode file mode specifier, see {@code fopen} for valid values
 * @return a FILE pointer or {@code nullptr} on failure
 */
FILE* OpenFile(JSContext* cx, const char* filename, const char* mode);

/**
 * Read {@code length} bytes in the given buffer.
 *
 * @param cx current js-context
 * @param filename file name encoded in UTF-8, only used for error reporting
 * @param file file pointer to read from
 * @param buffer destination buffer to copy read bytes into
 * @param length number of bytes to read
 * @return returns false and reports an error if not exactly {@code length}
 *         bytes could be read from the input file
 */
bool ReadFile(JSContext* cx, const char* filename, FILE* file, char* buffer,
              size_t length);

/**
 * Compute the file size in bytes.
 *
 * @param cx current js-context
 * @param filename file name encoded in UTF-8, only used for error reporting
 * @param file file object to inspect
 * @param size output parameter to store the file size into
 * @return returns false and reports an error if an I/O error occurred
 */
bool FileSize(JSContext* cx, const char* filename, FILE* file, size_t* size);

/**
 * Return the system error message for the given error number. The error
 * message is UTF-8 encoded.
 *
 * @param cx current js-context
 * @param errnum error number
 * @return error message or {@code nullptr} on error
 */
JS::UniqueChars SystemErrorMessage(JSContext* cx, int errnum);

}  // namespace shell
}  // namespace js

#endif /* shell_OSObject_h */
