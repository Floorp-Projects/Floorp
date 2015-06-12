/*
*******************************************************************************
* Copyright (C) 2014, International Business Machines Corporation and         *
* others. All Rights Reserved.                                                *
*******************************************************************************
*
* File SIMPLEPATTERNFORMATTERTEST.CPP
*
********************************************************************************
*/
#include "cstring.h"
#include "intltest.h"
#include "simplepatternformatter.h"

class SimplePatternFormatterTest : public IntlTest {
public:
    SimplePatternFormatterTest() {
    }
    void TestNoPlaceholders();
    void TestOnePlaceholder();
    void TestManyPlaceholders();
    void TestTooFewPlaceholderValues();
    void TestBadArguments();
    void TestGetPatternWithNoPlaceholders();
    void TestFormatReplaceNoOptimization();
    void TestFormatReplaceNoOptimizationLeadingText();
    void TestFormatReplaceOptimization();
    void TestFormatReplaceNoOptimizationLeadingPlaceholderUsedTwice();
    void TestFormatReplaceOptimizationNoOffsets();
    void TestFormatReplaceNoOptimizationNoOffsets();
    void runIndexedTest(int32_t index, UBool exec, const char *&name, char *par=0);
private:
    void verifyOffsets(
            const int32_t *expected,
            const int32_t *actual,
            int32_t count);
};

void SimplePatternFormatterTest::runIndexedTest(int32_t index, UBool exec, const char* &name, char* /*par*/) {
  TESTCASE_AUTO_BEGIN;
  TESTCASE_AUTO(TestNoPlaceholders);
  TESTCASE_AUTO(TestOnePlaceholder);
  TESTCASE_AUTO(TestManyPlaceholders);
  TESTCASE_AUTO(TestTooFewPlaceholderValues);
  TESTCASE_AUTO(TestBadArguments);
  TESTCASE_AUTO(TestGetPatternWithNoPlaceholders);
  TESTCASE_AUTO(TestFormatReplaceNoOptimization);
  TESTCASE_AUTO(TestFormatReplaceNoOptimizationLeadingText);
  TESTCASE_AUTO(TestFormatReplaceOptimization);
  TESTCASE_AUTO(TestFormatReplaceNoOptimizationLeadingPlaceholderUsedTwice);
  TESTCASE_AUTO(TestFormatReplaceOptimizationNoOffsets);
  TESTCASE_AUTO(TestFormatReplaceNoOptimizationNoOffsets);
  TESTCASE_AUTO_END;
}

void SimplePatternFormatterTest::TestNoPlaceholders() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt("This doesn''t have templates '{0}");
    assertEquals("PlaceholderCount", 0, fmt.getPlaceholderCount());
    UnicodeString appendTo;
    assertEquals(
            "format",
            "This doesn't have templates {0}", 
            fmt.format("unused", appendTo, status));
    fmt.compile("This has {} bad {012d placeholders", status);
    assertEquals("PlaceholderCount", 0, fmt.getPlaceholderCount());
    appendTo.remove();
    assertEquals(
            "format",
            "This has {} bad {012d placeholders", 
            fmt.format("unused", appendTo, status));
    assertSuccess("Status", status);
}

void SimplePatternFormatterTest::TestOnePlaceholder() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile("{0} meter", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    assertEquals("PlaceholderCount", 1, fmt.getPlaceholderCount());
    UnicodeString appendTo;
    assertEquals(
            "format",
            "1 meter",
            fmt.format("1", appendTo, status));

    // assignment
    SimplePatternFormatter s;
    s = fmt;
    appendTo.remove();
    assertEquals(
            "Assignment",
            "1 meter",
            s.format("1", appendTo, status));

    // Copy constructor
    SimplePatternFormatter r(fmt);
    appendTo.remove();
    assertEquals(
            "Copy constructor",
            "1 meter",
            r.format("1", appendTo, status));
    assertSuccess("Status", status);
}

void SimplePatternFormatterTest::TestManyPlaceholders() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile(
            "Templates {2}{1}{5} and {4} are out of order.", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    assertEquals("PlaceholderCount", 6, fmt.getPlaceholderCount());
    UnicodeString values[] = {
            "freddy", "tommy", "frog", "billy", "leg", "{0}"};
    UnicodeString *params[] = {
           &values[0], &values[1], &values[2], &values[3], &values[4], &values[5]}; 
    int32_t offsets[6];
    int32_t expectedOffsets[6] = {-1, 22, 18, -1, 35, 27};
    UnicodeString appendTo("Prefix: ");
    assertEquals(
            "format",
            "Prefix: Templates frogtommy{0} and leg are out of order.",
            fmt.formatAndAppend(
                    params,
                    UPRV_LENGTHOF(params),
                    appendTo,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
    appendTo.remove();

    // Ensure we don't write to offsets array beyond its length.
    status = U_ZERO_ERROR;
    offsets[UPRV_LENGTHOF(offsets) - 1] = 289;
    appendTo.remove();
    fmt.formatAndAppend(
            params,
            UPRV_LENGTHOF(params),
            appendTo,
            offsets,
            UPRV_LENGTHOF(offsets) - 1,
            status);
    assertEquals("Offsets buffer length", 289, offsets[UPRV_LENGTHOF(offsets) - 1]);

    // Test assignment
    SimplePatternFormatter s;
    s = fmt;
    appendTo.remove();
    assertEquals(
            "Assignment",
            "Templates frogtommy{0} and leg are out of order.",
            s.formatAndAppend(
                    params,
                    UPRV_LENGTHOF(params),
                    appendTo,
                    NULL,
                    0,
                    status));

    // Copy constructor
    SimplePatternFormatter r(fmt);
    appendTo.remove();
    assertEquals(
            "Copy constructor",
            "Templates frogtommy{0} and leg are out of order.",
            r.formatAndAppend(
                    params,
                    UPRV_LENGTHOF(params),
                    appendTo,
                    NULL,
                    0,
                    status));
    r.compile("{0} meter", status);
    assertEquals("PlaceholderCount", 1, r.getPlaceholderCount());
    appendTo.remove();
    assertEquals(
            "Replace with new compile",
            "freddy meter",
            r.format("freddy", appendTo, status));
    r.compile("{0}, {1}", status);
    assertEquals("PlaceholderCount", 2, r.getPlaceholderCount());
    appendTo.remove();
    assertEquals(
            "2 arg",
            "foo, bar",
            r.format("foo", "bar", appendTo, status));
    r.compile("{0}, {1} and {2}", status);
    assertEquals("PlaceholderCount", 3, r.getPlaceholderCount());
    appendTo.remove();
    assertEquals(
            "3 arg",
            "foo, bar and baz",
            r.format("foo", "bar", "baz", appendTo, status));
    assertSuccess("Status", status);
}

void SimplePatternFormatterTest::TestTooFewPlaceholderValues() {
    SimplePatternFormatter fmt("{0} and {1}");
    UnicodeString appendTo;
    UnicodeString firstValue;
    UnicodeString *params[] = {&firstValue};

    UErrorCode status = U_ZERO_ERROR;
    fmt.format(
            firstValue, appendTo, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }

    status = U_ZERO_ERROR;
    fmt.formatAndAppend(
            params, UPRV_LENGTHOF(params), appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }

    status = U_ZERO_ERROR;
    fmt.formatAndReplace(
            params, UPRV_LENGTHOF(params), appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
}

void SimplePatternFormatterTest::TestBadArguments() {
    SimplePatternFormatter fmt("pickle");
    UnicodeString appendTo;
    UErrorCode status = U_ZERO_ERROR;

    // These succeed
    fmt.formatAndAppend(
            NULL, 0, appendTo, NULL, 0, status);
    fmt.formatAndReplace(
            NULL, 0, appendTo, NULL, 0, status);
    assertSuccess("", status);
    status = U_ZERO_ERROR;

    // fails
    fmt.formatAndAppend(
            NULL, 1, appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
    status = U_ZERO_ERROR;
   
    // fails
    fmt.formatAndAppend(
            NULL, 0, appendTo, NULL, 1, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
    status = U_ZERO_ERROR;

    // fails because appendTo used as a parameter value
    const UnicodeString *params[] = {&appendTo};
    fmt.formatAndAppend(
            params, UPRV_LENGTHOF(params), appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
    status = U_ZERO_ERROR;

   
    // fails
    fmt.formatAndReplace(
            NULL, 1, appendTo, NULL, 0, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
    status = U_ZERO_ERROR;
   
    // fails
    fmt.formatAndReplace(
            NULL, 0, appendTo, NULL, 1, status);
    if (status != U_ILLEGAL_ARGUMENT_ERROR) {
        errln("Expected U_ILLEGAL_ARGUMENT_ERROR");
    }
}

void SimplePatternFormatterTest::TestGetPatternWithNoPlaceholders() {
    SimplePatternFormatter fmt("{0} has no {1} placeholders.");
    assertEquals(
            "", " has no  placeholders.", fmt.getPatternWithNoPlaceholders());
}

void SimplePatternFormatterTest::TestFormatReplaceNoOptimization() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile("{2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&result, &freddy, &frog, &by};
    assertEquals(
            "",
            "frog, original, freddy and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {6, 16, 0, 27};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimplePatternFormatterTest::TestFormatReplaceNoOptimizationLeadingText() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile("boo {2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "boo original, freddy, frog and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {14, 22, 4, 31};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimplePatternFormatterTest::TestFormatReplaceOptimization() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile("{2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "original, freddy, frog and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {10, 18, 0, 27};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimplePatternFormatterTest::TestFormatReplaceNoOptimizationLeadingPlaceholderUsedTwice() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile("{2}, {0}, {1} and {3} {2}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    int offsets[4];
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "original, freddy, frog and by original",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    offsets,
                    UPRV_LENGTHOF(offsets),
                    status));
    if (!assertSuccess("Status", status)) {
        return;
    }
    int32_t expectedOffsets[] = {10, 18, 30, 27};
    verifyOffsets(expectedOffsets, offsets, UPRV_LENGTHOF(expectedOffsets));
}

void SimplePatternFormatterTest::TestFormatReplaceOptimizationNoOffsets() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt;
    fmt.compile("{2}, {0}, {1} and {3}", status);
    if (!assertSuccess("Status", status)) {
        return;
    }
    UnicodeString result("original");
    UnicodeString freddy("freddy");
    UnicodeString frog("frog");
    UnicodeString by("by");
    const UnicodeString *params[] = {&freddy, &frog, &result, &by};
    assertEquals(
            "",
            "original, freddy, frog and by",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    NULL,
                    0,
                    status));
    assertSuccess("Status", status);
}

void SimplePatternFormatterTest::TestFormatReplaceNoOptimizationNoOffsets() {
    UErrorCode status = U_ZERO_ERROR;
    SimplePatternFormatter fmt("Placeholders {0} and {1}");
    UnicodeString result("previous:");
    UnicodeString frog("frog");
    const UnicodeString *params[] = {&result, &frog};
    assertEquals(
            "",
            "Placeholders previous: and frog",
            fmt.formatAndReplace(
                    params,
                    UPRV_LENGTHOF(params),
                    result,
                    NULL,
                    0,
                    status));
    assertSuccess("Status", status);
}

void SimplePatternFormatterTest::verifyOffsets(
        const int32_t *expected, const int32_t *actual, int32_t count) {
    for (int32_t i = 0; i < count; ++i) {
        if (expected[i] != actual[i]) {
            errln("Expected %d, got %d", expected[i], actual[i]);
        }
    }
}

extern IntlTest *createSimplePatternFormatterTest() {
    return new SimplePatternFormatterTest();
}

