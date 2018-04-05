/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: set ts=8 sts=4 et sw=4 tw=99:
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <string.h>
#include <sys/stat.h>

#if defined (XP_WIN)
#include <windows.h>
#elif defined(XP_UNIX)
#include <fcntl.h>
#include <unistd.h>
#endif // defined (XP_WIN) || defined (XP_UNIX)

#include "mozilla/Maybe.h"

#include "frontend/BinTokenReaderTester.h"
#include "gc/Zone.h"

#include "js/Vector.h"

#include "jsapi-tests/tests.h"

using mozilla::Maybe;

using Tokenizer = js::frontend::BinTokenReaderTester;
using Chars = js::frontend::BinTokenReaderTester::Chars;

// Hack: These tests need access to resources, which are present in the source dir
// but not copied by our build system. To simplify things, we chdir to the source
// dir at the start of each test and return to the previous directory afterwards.

#if defined(XP_UNIX)

#include <sys/param.h>

static int gJsDirectory(0);
void enterJsDirectory() {
// Save current directory.
    MOZ_ASSERT(gJsDirectory == 0);
    gJsDirectory = open(".", O_RDONLY);
    MOZ_ASSERT(gJsDirectory != 0, "Could not open directory '.'");
// Go to the directory provided by the test harness, if any.
    const char* destination = getenv("CPP_UNIT_TESTS_DIR_JS_SRC");
    if (destination) {
        if (chdir(destination) == -1)
            MOZ_CRASH_UNSAFE_PRINTF("Could not chdir to %s", destination);
    }
}

void exitJsDirectory() {
    MOZ_ASSERT(gJsDirectory);
    if (fchdir(gJsDirectory) == -1)
        MOZ_CRASH("Could not return to original directory");
    if (close(gJsDirectory) != 0)
        MOZ_CRASH("Could not close js directory");
    gJsDirectory = 0;
}

#else

char gJsDirectory[MAX_PATH] = { 0 };

void enterJsDirectory() {
    // Save current directory.
    MOZ_ASSERT(strlen(gJsDirectory) == 0);
    auto result = GetCurrentDirectory(MAX_PATH, gJsDirectory);
    if (result <= 0)
        MOZ_CRASH("Could not get current directory");
    if (result > MAX_PATH)
        MOZ_CRASH_UNSAFE_PRINTF("Could not get current directory: needed %ld bytes, got %ld\n", result, MAX_PATH);

    // Find destination directory, if any.
    char destination[MAX_PATH];
    result = GetEnvironmentVariable("CPP_UNIT_TESTS_DIR_JS_SRC", destination, MAX_PATH);
    if (result == 0) {
        if (GetLastError() == ERROR_ENVVAR_NOT_FOUND)
            return; // No need to chdir
        else
            MOZ_CRASH("Could not get CPP_UNIT_TESTS_DIR_JS_SRC");
    }
    if (result > MAX_PATH) {
        MOZ_CRASH_UNSAFE_PRINTF("Could not get CPP_UNIT_TESTS_DIR_JS_SRC: needed %ld bytes, got %ld\n", result, MAX_PATH);
    }

    // Go to the directory.
    if (SetCurrentDirectory(destination) == 0)
        MOZ_CRASH_UNSAFE_PRINTF("Could not chdir to %s", destination);
}

void exitJsDirectory() {
    MOZ_ASSERT(strlen(gJsDirectory) > 0);
    if (SetCurrentDirectory(gJsDirectory) == 0)
        MOZ_CRASH("Could not return to original directory");
    gJsDirectory[0] = 0;
}

#endif // defined(XP_UNIX) || defined(XP_WIN)

void readFull(const char* path, js::Vector<uint8_t>& buf) {
    enterJsDirectory();
    buf.shrinkTo(0);
    FILE* in = fopen(path, "rb");
    if (!in)
        MOZ_CRASH_UNSAFE_PRINTF("Could not open %s: %s", path, strerror(errno));

    struct stat info;
    if (stat(path, &info) < 0)
        MOZ_CRASH_UNSAFE_PRINTF("Could not get stat on %s", path);

    if (!buf.growBy(info.st_size))
        MOZ_CRASH("OOM");

    int result = fread(buf.begin(), 1, info.st_size, in);
    if (fclose(in) != 0)
        MOZ_CRASH("Could not close input file");
    if (result != info.st_size)
        MOZ_CRASH_UNSAFE_PRINTF("Read error while reading %s: expected %llu bytes, got %llu", path, (unsigned long long)info.st_size, (unsigned long long)result);
    exitJsDirectory();
}


// Reading a simple string.
BEGIN_TEST(testBinTokenReaderTesterSimpleString)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-simple-string.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    Chars found(cx);
    CHECK(tokenizer.readChars(found).isOk());

    CHECK(Tokenizer::equals(found, "simple string")); // FIXME: Find a way to make CHECK_EQUAL use `Tokenizer::equals`.

    return true;
}
END_TEST(testBinTokenReaderTesterSimpleString)

// Reading a string with embedded 0.
BEGIN_TEST(testBinTokenReaderTesterStringWithEscapes)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-string-with-escapes.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    Chars found(cx);
    CHECK(tokenizer.readChars(found).isOk());

    CHECK(Tokenizer::equals(found, "string with escapes \0\1\0")); // FIXME: Find a way to make CHECK_EQUAL use `Tokenizer::equals`.

    return true;
}
END_TEST(testBinTokenReaderTesterStringWithEscapes)

// Reading an empty untagged tuple
BEGIN_TEST(testBinTokenReaderTesterEmptyUntaggedTuple)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-empty-untagged-tuple.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    {
        Tokenizer::AutoTuple guard(tokenizer);
        CHECK(tokenizer.enterUntaggedTuple(guard).isOk());
        CHECK(guard.done().isOk());
    }

    return true;
}
END_TEST(testBinTokenReaderTesterEmptyUntaggedTuple)

// Reading a untagged tuple with two strings
BEGIN_TEST(testBinTokenReaderTesterTwoStringsInTuple)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-trivial-untagged-tuple.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    {
        Tokenizer::AutoTuple guard(tokenizer);
        CHECK(tokenizer.enterUntaggedTuple(guard).isOk());

        Chars found_0(cx);
        CHECK(tokenizer.readChars(found_0).isOk());
        CHECK(Tokenizer::equals(found_0, "foo")); // FIXME: Find a way to make CHECK_EQUAL use `Tokenizer::equals`.

        Chars found_1(cx);
        CHECK(tokenizer.readChars(found_1).isOk());
        CHECK(Tokenizer::equals(found_1, "bar")); // FIXME: Find a way to make CHECK_EQUAL use `Tokenizer::equals`.

        CHECK(guard.done().isOk());
    }

    return true;
}
END_TEST(testBinTokenReaderTesterTwoStringsInTuple)

// Reading a tagged tuple `Pattern { id: "foo", value: 3.1415}`
BEGIN_TEST(testBinTokenReaderTesterSimpleTaggedTuple)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-simple-tagged-tuple.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    {
        js::frontend::BinKind tag;
        Tokenizer::BinFields fields(cx);
        Tokenizer::AutoTaggedTuple guard(tokenizer);
        CHECK(tokenizer.enterTaggedTuple(tag, fields, guard).isOk());

        CHECK(tag == js::frontend::BinKind::BindingIdentifier);

        Chars found_id(cx);
        const double EXPECTED_value = 3.1415;

        // Order of fields is deterministic.
        CHECK(fields[0] == js::frontend::BinField::Label);
        CHECK(fields[1] == js::frontend::BinField::Value);
        CHECK(tokenizer.readChars(found_id).isOk());
        double found_value = tokenizer.readDouble().unwrap();

        CHECK(EXPECTED_value == found_value); // Apparently, CHECK_EQUAL doesn't work on `double`.
        CHECK(Tokenizer::equals(found_id, "foo"));
        CHECK(guard.done().isOk());
    }

    return true;
}
END_TEST(testBinTokenReaderTesterSimpleTaggedTuple)


// Reading an empty list
BEGIN_TEST(testBinTokenReaderTesterEmptyList)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-empty-list.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    {
        uint32_t length;
        Tokenizer::AutoList guard(tokenizer);
        CHECK(tokenizer.enterList(length, guard).isOk());

        CHECK(length == 0);
        CHECK(guard.done().isOk());
    }

    return true;
}
END_TEST(testBinTokenReaderTesterEmptyList)

// Reading `["foo", "bar"]`
BEGIN_TEST(testBinTokenReaderTesterSimpleList)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-trivial-list.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    {
        uint32_t length;
        Tokenizer::AutoList guard(tokenizer);
        CHECK(tokenizer.enterList(length, guard).isOk());

        CHECK(length == 2);

        Chars found_0(cx);
        CHECK(tokenizer.readChars(found_0).isOk());
        CHECK(Tokenizer::equals(found_0, "foo"));

        Chars found_1(cx);
        CHECK(tokenizer.readChars(found_1).isOk());
        CHECK(Tokenizer::equals(found_1, "bar"));

        CHECK(guard.done().isOk());
    }

    return true;
}
END_TEST(testBinTokenReaderTesterSimpleList)


// Reading `[["foo", "bar"]]`
BEGIN_TEST(testBinTokenReaderTesterNestedList)
{
    js::Vector<uint8_t> contents(cx);
    readFull("jsapi-tests/binast/tokenizer/tester/test-nested-lists.binjs", contents);
    Tokenizer tokenizer(cx, contents);

    {
        uint32_t outerLength;
        Tokenizer::AutoList outerGuard(tokenizer);
        CHECK(tokenizer.enterList(outerLength, outerGuard).isOk());
        CHECK_EQUAL(outerLength, (uint32_t)1);

        {
            uint32_t innerLength;
            Tokenizer::AutoList innerGuard(tokenizer);
            CHECK(tokenizer.enterList(innerLength, innerGuard).isOk());
            CHECK_EQUAL(innerLength, (uint32_t)2);

            Chars found_0(cx);
            CHECK(tokenizer.readChars(found_0).isOk());
            CHECK(Tokenizer::equals(found_0, "foo"));

            Chars found_1(cx);
            CHECK(tokenizer.readChars(found_1).isOk());
            CHECK(Tokenizer::equals(found_1, "bar"));

            CHECK(innerGuard.done().isOk());
        }

        CHECK(outerGuard.done().isOk());
    }

    return true;
}
END_TEST(testBinTokenReaderTesterNestedList)
