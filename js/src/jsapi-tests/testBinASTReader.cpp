/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: set ts=8 sts=2 et sw=2 tw=80:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#if defined(XP_UNIX)

#  include <dirent.h>
#  include <fcntl.h>
#  include <sys/stat.h>
#  include <unistd.h>

#elif defined(XP_WIN)

#  include <windows.h>

#endif

#include "mozilla/Maybe.h"

#include "jsapi.h"

#include "frontend/BinASTParser.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseContext.h"
#include "frontend/Parser.h"
#include "js/Exception.h"
#include "js/Vector.h"

#include "jsapi-tests/tests.h"

#include "vm/Interpreter.h"

using UsedNameTracker = js::frontend::UsedNameTracker;
using namespace JS;
using namespace js;

// Hack: These tests need access to resources, which are present in the source
// dir but not copied by our build system. To simplify things, we chdir to the
// source dir at the start of each test and return to the previous directory
// afterwards.

#if defined(XP_UNIX)

#  include <sys/param.h>

static int gJsDirectory(0);
void enterJsDirectory() {
  // Save current directory.
  MOZ_ASSERT(gJsDirectory == 0);
  gJsDirectory = open(".", O_RDONLY);
  MOZ_ASSERT(gJsDirectory != 0, "Could not open directory '.'");
  // Go to the directory provided by the test harness, if any.
  const char* destination = getenv("CPP_UNIT_TESTS_DIR_JS_SRC");
  if (destination) {
    if (chdir(destination) == -1) {
      MOZ_CRASH_UNSAFE_PRINTF("Could not chdir to %s", destination);
    }
  }
}

void exitJsDirectory() {
  MOZ_ASSERT(gJsDirectory);
  if (fchdir(gJsDirectory) == -1) {
    MOZ_CRASH("Could not return to original directory");
  }
  if (close(gJsDirectory) != 0) {
    MOZ_CRASH("Could not close js directory");
  }
  gJsDirectory = 0;
}

#else

char gJsDirectory[MAX_PATH] = {0};

void enterJsDirectory() {
  // Save current directory.
  MOZ_ASSERT(strlen(gJsDirectory) == 0);
  auto result = GetCurrentDirectory(MAX_PATH, gJsDirectory);
  if (result <= 0) {
    MOZ_CRASH("Could not get current directory");
  }
  if (result > MAX_PATH) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Could not get current directory: needed %ld bytes, got %ld\n", result,
        MAX_PATH);
  }

  // Find destination directory, if any.
  char destination[MAX_PATH];
  result = GetEnvironmentVariable("CPP_UNIT_TESTS_DIR_JS_SRC", destination,
                                  MAX_PATH);
  if (result == 0) {
    if (GetLastError() == ERROR_ENVVAR_NOT_FOUND) {
      return;  // No need to chdir
    } else {
      MOZ_CRASH("Could not get CPP_UNIT_TESTS_DIR_JS_SRC");
    }
  }
  if (result > MAX_PATH) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Could not get CPP_UNIT_TESTS_DIR_JS_SRC: needed %ld bytes, got %ld\n",
        result, MAX_PATH);
  }

  // Go to the directory.
  if (SetCurrentDirectory(destination) == 0) {
    MOZ_CRASH_UNSAFE_PRINTF("Could not chdir to %s", destination);
  }
}

void exitJsDirectory() {
  MOZ_ASSERT(strlen(gJsDirectory) > 0);
  if (SetCurrentDirectory(gJsDirectory) == 0) {
    MOZ_CRASH("Could not return to original directory");
  }
  gJsDirectory[0] = 0;
}

#endif  // defined(XP_UNIX) || defined(XP_WIN)

// Read a full file (binary).
void readFull(const char* path, js::Vector<uint8_t>& buf) {
  enterJsDirectory();
  buf.shrinkTo(0);
  FILE* in = fopen(path, "rb");
  if (!in) {
    MOZ_CRASH_UNSAFE_PRINTF("Could not open %s: %s", path, strerror(errno));
  }

  struct stat info;
  if (stat(path, &info) < 0) {
    MOZ_CRASH_UNSAFE_PRINTF("Could not get stat on %s", path);
  }

  if (!buf.growBy(info.st_size)) {
    MOZ_CRASH("OOM");
  }

  int result = fread(buf.begin(), 1, info.st_size, in);
  if (fclose(in) != 0) {
    MOZ_CRASH("Could not close input file");
  }
  if (result != info.st_size) {
    MOZ_CRASH_UNSAFE_PRINTF(
        "Read error while reading %s: expected %llu bytes, got %llu", path,
        (unsigned long long)info.st_size, (unsigned long long)result);
  }
  exitJsDirectory();
}

// Read a full file (text)
void readFull(JSContext* cx, const char* path,
              js::Vector<mozilla::Utf8Unit>& buf) {
  buf.shrinkTo(0);

  js::Vector<uint8_t> intermediate(cx);
  readFull(path, intermediate);

  if (!buf.appendAll(intermediate)) {
    MOZ_CRASH("Couldn't read data");
  }
}

// Invariant: `path` must end with directory separator.
template <typename Tok>
void runTestFromPath(JSContext* cx, const char* path) {
  const char BIN_SUFFIX[] = ".binjs";
  const char TXT_SUFFIX[] = ".js";
  fprintf(stderr, "runTestFromPath: entering directory '%s'\n", path);
  const size_t pathlen = strlen(path);

#if defined(XP_UNIX)
  MOZ_ASSERT(path[pathlen - 1] == '/');

  // Read the list of files in the directory.
  enterJsDirectory();
  DIR* dir = opendir(path);
  exitJsDirectory();
  if (!dir) {
    MOZ_CRASH("Couldn't open directory");
  }

  while (auto entry = readdir(dir)) {
    const char* d_name = entry->d_name;
    const bool isDirectory = entry->d_type == DT_DIR;

#elif defined(XP_WIN)
  MOZ_ASSERT(path[pathlen - 1] == '\\');

  Vector<char> pattern(cx);
  if (!pattern.append(path, pathlen)) {
    MOZ_CRASH();
  }
  if (!pattern.append('*')) {
    MOZ_CRASH();
  }
  if (!pattern.append('\0')) {
    MOZ_CRASH();
  }

  WIN32_FIND_DATA FindFileData;
  enterJsDirectory();
  HANDLE hFind = FindFirstFile(pattern.begin(), &FindFileData);
  exitJsDirectory();
  for (bool found = (hFind != INVALID_HANDLE_VALUE); found;
       found = FindNextFile(hFind, &FindFileData)) {
    const char* d_name = FindFileData.cFileName;
    const bool isDirectory =
        FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY;

#endif  // defined(XP_UNIX) || defined(XP_WIN)

    const size_t namlen = strlen(d_name);

    // Recurse through subdirectories.
    if (isDirectory) {
      if (strcmp(d_name, ".") == 0) {
        continue;
      }
      if (strcmp(d_name, "..") == 0) {
        continue;
      }

      Vector<char> subPath(cx);
      // Start with `path` (including directory separator).
      if (!subPath.append(path, pathlen)) {
        MOZ_CRASH();
      }
      if (!subPath.append(d_name, namlen)) {
        MOZ_CRASH();
      }
      // Append same directory separator.
      if (!subPath.append(path[pathlen - 1])) {
        MOZ_CRASH();
      }
      if (!subPath.append(0)) {
        MOZ_CRASH();
      }
      runTestFromPath<Tok>(cx, subPath.begin());
      continue;
    }

    // Make sure that we run GC between two tests. Otherwise, since we're
    // running everything from the same cx and without returning to JS, there is
    // nothing to deallocate the ASTs.
    JS_GC(cx);

    LifoAllocScope allocScope(&cx->tempLifoAlloc());

    // Find files whose name ends with ".binjs".
    fprintf(stderr, "Considering %s\n", d_name);
    if (namlen < sizeof(BIN_SUFFIX)) {
      continue;
    }
    if (strncmp(d_name + namlen - (sizeof(BIN_SUFFIX) - 1), BIN_SUFFIX,
                sizeof(BIN_SUFFIX)) != 0)
      continue;

    // Find text file.
    Vector<char> txtPath(cx);
    if (!txtPath.append(path, pathlen)) {
      MOZ_CRASH();
    }
    if (!txtPath.append(d_name, namlen)) {
      MOZ_CRASH();
    }
    txtPath.shrinkBy(sizeof(BIN_SUFFIX) - 1);
    if (!txtPath.append(TXT_SUFFIX, sizeof(TXT_SUFFIX))) {
      MOZ_CRASH();
    }
    fprintf(stderr, "Testing %s\n", txtPath.begin());

    // Read text file.
    js::Vector<mozilla::Utf8Unit> txtSource(cx);
    readFull(cx, txtPath.begin(), txtSource);

    // Parse text file.
    CompileOptions txtOptions(cx);
    txtOptions.setFileAndLine(txtPath.begin(), 0);

    frontend::CompilationInfo compilationInfo(cx, allocScope, txtOptions);
    if (!compilationInfo.init(cx)) {
      MOZ_CRASH("Couldn't initialize CompilationInfo");
    }

    js::frontend::Parser<js::frontend::FullParseHandler, mozilla::Utf8Unit>
        txtParser(cx, txtOptions, txtSource.begin(), txtSource.length(),
                  /* foldConstants = */ false, compilationInfo, nullptr,
                  nullptr, compilationInfo.sourceObject);
    if (!txtParser.checkOptions()) {
      MOZ_CRASH("Bad options");
    }

    auto txtParsed =
        txtParser
            .parse();  // Will be deallocated once `parser` goes out of scope.

    ExceptionStack txtExn(cx);
    if (!txtParsed) {
      // Save exception for more detailed error message, if necessary.
      if (!StealPendingExceptionStack(cx, &txtExn)) {
        MOZ_CRASH("Couldn't clear exception");
      }
    }

    // Read binary file.
    Vector<char> binPath(cx);
    if (!binPath.append(path, pathlen)) {
      MOZ_CRASH();
    }
    if (!binPath.append(d_name, namlen)) {
      MOZ_CRASH();
    }
    if (!binPath.append(0)) {
      MOZ_CRASH();
    }

    js::Vector<uint8_t> binSource(cx);
    readFull(binPath.begin(), binSource);

    // Parse binary file.
    CompileOptions binOptions(cx);
    binOptions.setFileAndLine(binPath.begin(), 0);

    frontend::CompilationInfo binCompilationInfo(cx, allocScope, binOptions);
    if (!binCompilationInfo.init(cx)) {
      MOZ_CRASH();
    }

    frontend::Directives directives(false);
    frontend::GlobalSharedContext globalsc(cx, ScopeKind::Global,
                                           binCompilationInfo, directives);

    frontend::BinASTParser<Tok> binParser(cx, binCompilationInfo, binOptions,
                                          binCompilationInfo.sourceObject);

    auto binParsed = binParser.parse(
        &globalsc,
        binSource);  // Will be deallocated once `reader` goes out of scope.
    ExceptionStack binExn(cx);
    if (binParsed.isErr()) {
      // Save exception for more detailed error message, if necessary.
      if (!StealPendingExceptionStack(cx, &binExn)) {
        MOZ_CRASH("Couldn't clear binExn");
      }
    }

    // The binary parser should accept the file iff the text parser has.
    if (binParsed.isOk() && !txtParsed) {
      fprintf(stderr, "Text file parsing failed: ");

      js::ErrorReport report(cx);
      if (!report.init(cx, txtExn, js::ErrorReport::WithSideEffects)) {
        MOZ_CRASH("Couldn't report txtExn");
      }

      PrintError(cx, stderr, report.toStringResult(), report.report(),
                 /* reportWarnings */ true);
      MOZ_CRASH("Binary parser accepted a file that text parser rejected");
    }

    if (binParsed.isErr() && txtParsed) {
      fprintf(stderr, "Binary file parsing failed: ");

      js::ErrorReport report(cx);
      if (!report.init(cx, binExn, js::ErrorReport::WithSideEffects)) {
        MOZ_CRASH("Couldn't report binExn");
      }

      PrintError(cx, stderr, report.toStringResult(), report.report(),
                 /* reportWarnings */ true);
      MOZ_CRASH("Binary parser rejected a file that text parser accepted");
    }

    if (binParsed.isErr()) {
      fprintf(stderr,
              "Binary parser and text parser agree that %s is invalid\n",
              txtPath.begin());
      continue;
    }

#if defined(DEBUG)  // Dumping an AST is only defined in DEBUG builds
    // Compare ASTs.
    Sprinter binPrinter(cx);
    if (!binPrinter.init()) {
      MOZ_CRASH("Couldn't display binParsed");
    }
    DumpParseTree(binParsed.unwrap(), binPrinter);

    Sprinter txtPrinter(cx);
    if (!txtPrinter.init()) {
      MOZ_CRASH("Couldn't display txtParsed");
    }
    DumpParseTree(txtParsed, txtPrinter);

    if (strcmp(binPrinter.string(), txtPrinter.string()) != 0) {
      fprintf(stderr,
              "Got distinct ASTs when parsing %s "
              "(%p/%p):\n\tBINARY\n%s\n\n\tTEXT\n%s\n",
              txtPath.begin(), (void*)binPrinter.getOffset(),
              (void*)txtPrinter.getOffset(), binPrinter.string(),
              txtPrinter.string());
#  if 0   // Not for release, but useful for debugging.
          // In case of error, this dumps files to /tmp, so they may
          // easily be diffed.
            auto fd = open("/tmp/bin.ast", O_CREAT | O_TRUNC | O_WRONLY, 0666);
            if (!fd) {
                MOZ_CRASH("Could not open bin.ast");
            }
            auto result = write(fd, binPrinter.string(), binPrinter.stringEnd() - binPrinter.string());
            if (result <= 0) {
                MOZ_CRASH("Could not write to bin.ast");
            }
            result = close(fd);
            if (result != 0) {
                MOZ_CRASH("Could not close bin.ast");
            }

            fd = open("/tmp/txt.ast", O_CREAT | O_TRUNC | O_WRONLY, 0666);
            if (!fd) {
                MOZ_CRASH("Could not open txt.ast");
            }
            result = write(fd, txtPrinter.string(), txtPrinter.stringEnd() - txtPrinter.string());
            if (result <= 0) {
                MOZ_CRASH("Could not write to txt.ast");
            }
            result = close(fd);
            if (result != 0) {
                MOZ_CRASH("Could not close txt.ast");
            }
#  endif  // 0
      MOZ_CRASH("Got distinct ASTs");
    }

    fprintf(stderr, "Got the same AST when parsing %s\n", txtPath.begin());
#endif  // defined(DEBUG)
  }

#if defined(XP_WIN)
  if (!FindClose(hFind)) {
    MOZ_CRASH("Could not close Find");
  }
#elif defined(XP_UNIX)
  if (closedir(dir) != 0) {
    MOZ_CRASH("Could not close dir");
  }
#endif  // defined(XP_WIN)
}

BEGIN_TEST(testBinASTReaderMultipartECMAScript2) {
#if defined(XP_WIN)
  runTestFromPath<js::frontend::BinASTTokenReaderMultipart>(
      cx, "jsapi-tests\\binast\\parser\\multipart\\");
#else
  runTestFromPath<js::frontend::BinASTTokenReaderMultipart>(
      cx, "jsapi-tests/binast/parser/multipart/");
#endif  // defined(XP_XIN)
  return true;
}
END_TEST(testBinASTReaderMultipartECMAScript2)

BEGIN_TEST(testBinASTReaderContextECMAScript2) {
#if defined(XP_WIN)
  runTestFromPath<js::frontend::BinASTTokenReaderContext>(
      cx, "jsapi-tests\\binast\\parser\\context\\");
#else
  runTestFromPath<js::frontend::BinASTTokenReaderContext>(
      cx, "jsapi-tests/binast/parser/context/");
#endif  // defined(XP_XIN)
  return true;
}
END_TEST(testBinASTReaderContextECMAScript2)
