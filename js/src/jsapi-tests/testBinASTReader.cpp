/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#if defined(XP_UNIX)

#include <dirent.h>
#include <sys/stat.h>

#elif defined(XP_WIN)

#include <windows.h>

#endif

#include "jsapi.h"


#include "frontend/BinSource.h"
#include "frontend/FullParseHandler.h"
#include "frontend/ParseContext.h"
#include "frontend/Parser.h"
#include "gc/Zone.h"
#include "js/Vector.h"

#include "jsapi-tests/tests.h"

#include "vm/Interpreter.h"

using UsedNameTracker = js::frontend::UsedNameTracker;
using namespace JS;
using namespace js;

extern void enterJsDirectory();
extern void exitJsDirectory();
extern void readFull(const char* path, js::Vector<uint8_t>& buf);

void
readFull(JSContext* cx, const char* path, js::Vector<char16_t>& buf)
{
    buf.shrinkTo(0);

    js::Vector<uint8_t> intermediate(cx);
    readFull(path, intermediate);

    if (!buf.appendAll(intermediate))
        MOZ_CRASH("Couldn't read data");
}

BEGIN_TEST(testBinASTReaderECMAScript2)
{
    const char BIN_SUFFIX[] = ".binjs";
    const char TXT_SUFFIX[] = ".js";

    CompileOptions options(cx);
    options.setIntroductionType("unit test parse")
           .setFileAndLine("<string>", 1);

#if defined(XP_UNIX)

    const char PATH[] = "jsapi-tests/binast/parser/tester/";

    // Read the list of files in the directory.
    enterJsDirectory();
    DIR* dir = opendir(PATH);
    exitJsDirectory();
    if (!dir)
        MOZ_CRASH("Couldn't open directory");


    while (auto entry = readdir(dir)) {
        // Find files whose name ends with ".binjs".
        const char* d_name = entry->d_name;

#elif defined(XP_WIN)

    const char PATTERN[] = "jsapi-tests\\binast\\parser\\tester\\*.binjs";
    const char PATH[] = "jsapi-tests\\binast\\parser\\tester\\";

    WIN32_FIND_DATA FindFileData;
    enterJsDirectory();
    HANDLE hFind = FindFirstFile(PATTERN, &FindFileData);
    exitJsDirectory();
    for (bool found = (hFind != INVALID_HANDLE_VALUE);
            found;
            found = FindNextFile(hFind, &FindFileData))
    {
        const char* d_name = FindFileData.cFileName;

#endif // defined(XP_UNIX) || defined(XP_WIN)

        const size_t namlen = strlen(d_name);
        if (namlen < sizeof(BIN_SUFFIX))
            continue;
        if (strncmp(d_name + namlen - (sizeof(BIN_SUFFIX) - 1), BIN_SUFFIX, sizeof(BIN_SUFFIX)) != 0)
            continue;

        // Find text file.
        UniqueChars txtPath(static_cast<char*>(js_malloc(namlen + sizeof(PATH) + 1)));
        strncpy(txtPath.get(), PATH, sizeof(PATH));
        strncpy(txtPath.get() + sizeof(PATH) - 1, d_name, namlen);
        strncpy(txtPath.get() + sizeof(PATH) + namlen - sizeof(BIN_SUFFIX), TXT_SUFFIX, sizeof(TXT_SUFFIX));
        txtPath[sizeof(PATH) + namlen - sizeof(BIN_SUFFIX) + sizeof(TXT_SUFFIX) - 1] = 0;
        fprintf(stderr, "Testing %s\n", txtPath.get());

        // Read text file.
        js::Vector<char16_t> txtSource(cx);
        readFull(cx, txtPath.get(), txtSource);

        // Parse text file.
        UsedNameTracker txtUsedNames(cx);
        if (!txtUsedNames.init())
            MOZ_CRASH("Couldn't initialize used names");
        js::frontend::Parser<js::frontend::FullParseHandler, char16_t> parser(cx, cx->tempLifoAlloc(), options, txtSource.begin(), txtSource.length(),
                                                  /* foldConstants = */ false, txtUsedNames, nullptr,
                                                  nullptr);
        if (!parser.checkOptions())
            MOZ_CRASH("Bad options");

        auto txtParsed = parser.parse(); // Will be deallocated once `parser` goes out of scope.
        RootedValue txtExn(cx);
        if (!txtParsed) {
            // Save exception for more detailed error message, if necessary.
            if (!js::GetAndClearException(cx, &txtExn))
                MOZ_CRASH("Couldn't clear exception");
        }

        // Read binary file.
        UniqueChars binPath(static_cast<char*>(js_malloc(namlen + sizeof(PATH) + 1)));
        strncpy(binPath.get(), PATH, sizeof(PATH));
        strncpy(binPath.get() + sizeof(PATH) - 1, d_name, namlen);
        binPath[namlen + sizeof(PATH) - 1] = 0;

        js::Vector<uint8_t> binSource(cx);
        readFull(binPath.get(), binSource);

        // Parse binary file.
        js::frontend::UsedNameTracker binUsedNames(cx);
        if (!binUsedNames.init())
            MOZ_CRASH("Couldn't initialized binUsedNames");

        js::frontend::BinASTParser reader(cx, cx->tempLifoAlloc(), binUsedNames, options);

        auto binParsed = reader.parse(binSource); // Will be deallocated once `reader` goes out of scope.
        RootedValue binExn(cx);
        if (binParsed.isErr()) {
            // Save exception for more detailed error message, if necessary.
            if (!js::GetAndClearException(cx, &binExn))
                MOZ_CRASH("Couldn't clear binExn");
        }

        // The binary parser should accept the file iff the text parser has.
        if (binParsed.isOk() && !txtParsed) {
            fprintf(stderr, "Text file parsing failed: ");

            js::ErrorReport report(cx);
            if (!report.init(cx, txtExn, js::ErrorReport::WithSideEffects))
                MOZ_CRASH("Couldn't report txtExn");

            PrintError(cx, stderr, report.toStringResult(), report.report(), /* reportWarnings */ true);
            MOZ_CRASH("Binary parser accepted a file that text parser rejected");
        }

        if (binParsed.isErr() && txtParsed) {
            fprintf(stderr, "Binary file parsing failed: ");

            js::ErrorReport report(cx);
            if (!report.init(cx, binExn, js::ErrorReport::WithSideEffects))
                MOZ_CRASH("Couldn't report binExn");

            PrintError(cx, stderr, report.toStringResult(), report.report(), /* reportWarnings */ true);
            MOZ_CRASH("Binary parser rejected a file that text parser accepted");
        }

        if (binParsed.isErr()) {
            fprintf(stderr, "Binary parser and text parser agree that %s is invalid\n", txtPath.get());
            continue;
        }

#if defined(DEBUG) // Dumping an AST is only defined in DEBUG builds
        // Compare ASTs.
        Sprinter binPrinter(cx);
        if (!binPrinter.init())
            MOZ_CRASH("Couldn't display binParsed");
        DumpParseTree(binParsed.unwrap(), binPrinter);

        Sprinter txtPrinter(cx);
        if (!txtPrinter.init())
            MOZ_CRASH("Couldn't display txtParsed");
        DumpParseTree(txtParsed, txtPrinter);

        if (strcmp(binPrinter.string(), txtPrinter.string()) != 0) {
            fprintf(stderr, "Got distinct ASTs when parsing %s:\n\tBINARY\n%s\n\n\tTEXT\n%s\n", txtPath.get(), binPrinter.string(), txtPrinter.string());
            MOZ_CRASH("Got distinct ASTs");
        }
        fprintf(stderr, "Got the same AST when parsing %s\n", txtPath.get());

#endif // defined(DEBUG)
    }

#if defined(XP_WIN)
    if (!FindClose(hFind))
        MOZ_CRASH("Could not close Find");
#elif defined(XP_UNIX)
    if (closedir(dir) != 0)
        MOZ_CRASH("Could not close dir");
#endif // defined(XP_WIN)

    return true;
}
END_TEST(testBinASTReaderECMAScript2)

