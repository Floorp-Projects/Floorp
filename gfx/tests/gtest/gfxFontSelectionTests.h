/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 *
 * This file is #included directly by gfxFontSelectionTest.cpp, and as
 * such does not need any #include files or similar.  (However, should
 * any extra ones be required, it should be ok to do so, as well as
 * defining new functions, etc.
 *
 * To add a new test, call AddTest with the following arguments: the
 * CSS font-family string, the gfxFontStyle, an enum (either S_ASCII
 * or S_UTF8) indicating the string type, and then the text string
 * itself as a string literal.  Unfortunately there is no good way to
 * embed UTF8 directly into C code, so hex literals will need to be
 * placed in the string.  Because of the way \x is parsed things like
 * "\xabcd" won't work -- you have to do "\xab""cd".  "\xab\x01\x03"
 * will work fine, though.
 *
 * The result of AddTest should be assigned to the variable t; after
 * AddTest, one or more calls to t->Expect() should be added to define
 * the expected result.  Multiple Expect() calls in a row for the same
 * platform mean that the resulting glyph/font selection items needs
 * to have as many items as there are Expect() calls.  (See below for
 * examples.)
 * 
 * The arguments to Expect are:
 *
 *   platform - a string identifying the platform.
 *      Valid strings are "win32", "macosx", and "gtk2-pango".
 *   font - a string (UTF8) giving the unique name of the font.
 *      See below for how the unique name is constructed.
 *   glyphs - a set of glyph IDs that are expected.
 *      This array is constructed using a GLYPHS() macro.
 *
 * GLYPHS() is just a #define for LiteralArray, which is defined
 * in gfxFontSelectionTest.cpp -- if you need more array elements
 * than available, just extend LiteralArray with a new constructor
 * with the required number of unsigned longs.
 *
 * The unique font name is a platform-specific constructed string for
 * (mostly) identifying a font.  On Mac, it's created by taking the
 * Postscript name of the font.  On Windows, it's created by taking
 * the family name, and then appending attributes such as ":Bold",
 * ":Italic", etc.
 *
 * The easiest way to create a test is to add a call to AddTest, and
 * then run the test.  The output will include a list like:
 *
 * ==== Test 1
 *  expected:
 * Run[ 0]: 'Verdana' 73 82 82 
 * Run[ 1]: 'MS UI Gothic' 19401 
 * Run[ 2]: 'Verdana' 69 68 85 
 * Test 1 failed
 *
 * This gives you the information needed for the calls to Expect() --
 * the unique name, and the glyphs.  Appropriate calls to expect for
 * the above would be:
 *
 *   t->Expect ("win32", "Verdana", GLYPHS(73, 82, 82));
 *   t->Expect ("win32", "MS UI Gothic", GLYPHS(19401));
 *   t->Expect ("win32", "Verdana", GLYPHS(69, 68, 85));
 *
 */


void
SetupTests(nsTArray<TestEntry>& testList)
{
    TestEntry *t;

    /* some common styles */
    gfxFontStyle style_western_normal_16 (mozilla::gfx::FontStyle::NORMAL,
                                          400,
                                          0,
                                          16.0,
                                          NS_NewAtom(NS_LITERAL_STRING("en")),
                                          0.0,
                                          false, false,
                                          NS_LITERAL_STRING(""));

    gfxFontStyle style_western_bold_16 (mozilla::gfx::FontStyle::NORMAL,
                                        700,
                                        0,
                                        16.0,
                                        NS_NewAtom(NS_LITERAL_STRING("en")),
                                        0.0,
                                        false, false,
                                        NS_LITERAL_STRING(""));

    /* Test 0 */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 "ABCD");

    t->Expect ("win32", "Arial", GLYPHS(36, 37, 38, 39));
    t->Expect ("macosx", "Helvetica", GLYPHS(36, 37, 38, 39));
    t->Expect ("gtk2-pango", "Albany AMT", GLYPHS(36, 37, 38, 39));

    /* Test 1 */
    t = AddTest (testList, "verdana,sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 "foo\xe2\x80\x91""bar");

    t->Expect ("win32", "Verdana", GLYPHS(73, 82, 82));
    t->Expect ("win32", "Arial Unicode MS", GLYPHS(3236));
    t->Expect ("win32", "Verdana", GLYPHS(69, 68, 85));

    t->Expect ("macosx", "Verdana", GLYPHS(73, 82, 82));
    t->Expect ("macosx", "Helvetica", GLYPHS(587));
    t->Expect ("macosx", "Verdana", GLYPHS(69, 68, 85));

    /* Test 2 */
    t = AddTest (testList, "sans-serif",
                 style_western_bold_16,
                 S_ASCII,
                 "ABCD");

    t->Expect ("win32", "Arial:700", GLYPHS(36, 37, 38, 39));
    t->Expect ("macosx", "Helvetica-Bold", GLYPHS(36, 37, 38, 39));
    t->Expect ("gtk2-pango", "Albany AMT Bold", GLYPHS(36, 37, 38, 39));

    /* Test 3: RTL Arabic with a ligature and leading and trailing whitespace */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 " \xd8\xaa\xd9\x85 ");
    t->SetRTL();
    t->Expect ("macosx", "Helvetica", GLYPHS(3));
    t->Expect ("macosx", "ArialMT", GLYPHS(919, 993));
    t->Expect ("macosx", "Helvetica", GLYPHS(3));
    t->Expect ("win32", "Arial", GLYPHS(3, 919, 994, 3));

    /* Test 4: LTR Arabic with leading and trailing whitespace */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 " \xd9\x85\xd8\xaa ");
    t->Expect ("macosx", "Helvetica", GLYPHS(3));
    t->Expect ("macosx", "ArialMT", GLYPHS(993, 919));
    t->Expect ("macosx", "Helvetica", GLYPHS(3));
    t->Expect ("win32", "Arial", GLYPHS(3, 994, 919, 3));

    /* Test 5: RTL ASCII with leading whitespace */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 " ab");
    t->SetRTL();
    t->Expect ("macosx", "Helvetica", GLYPHS(3, 68, 69));
    t->Expect ("win32", "Arial", GLYPHS(3, 68, 69));
    t->Expect ("gtk2-pango", "Albany AMT", GLYPHS(3, 68, 69));

    /* Test 6: RTL ASCII with trailing whitespace */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 "ab ");
    t->SetRTL();
    t->Expect ("macosx", "Helvetica", GLYPHS(68, 69, 3));
    t->Expect ("win32", "Arial", GLYPHS(68, 69, 3));
    t->Expect ("gtk2-pango", "Albany AMT", GLYPHS(68, 69, 3));

    /* Test 7: Simple ASCII ligature */
    /* Do we have a Windows font with ligatures? Can we use DejaVu Sans? */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 "fi");
    t->Expect ("macosx", "Helvetica", GLYPHS(192));
    t->Expect ("win32", "Arial", GLYPHS(73, 76));

    /* Test 8: DEVANAGARI VOWEL I reordering */
    /* The glyph for DEVANAGARI VOWEL I 2367 (101) is displayed before the glyph for 2361 (99) */
    t = AddTest (testList, "sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 "\xe0\xa4\x9a\xe0\xa4\xbe\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\x8f"); // 2330 2366 2361 2367 2319
    t->Expect ("macosx", "DevanagariMT", GLYPHS(71, 100, 101, 99, 60));
    t->Expect ("win32", "Mangal", GLYPHS(133, 545, 465, 161, 102));

    // Disabled Test 9 & 10 because these appear to vary on mac

    /* Test 9: NWJ test */
    //t = AddTest (testList, "Kartika",
    //             style_western_normal_16,
    //             S_UTF8,
    //             "\xe0\xb4\xb3\xe0\xb5\x8d\xe2\x80\x8d");
    //t->Expect ("macosx", "MalayalamMN", GLYPHS(360));
    //t->Expect ("win32", "Kartika", GLYPHS(332));

    /* Test 10: NWJ fallback test */
    /* it isn't clear what we should actually do in this case.  Ideally
       we would have the same results as the previous test, but because
       we use sans-serif (i.e. Arial) CSS says we should should really
       use Arial for U+200D.
    */
    //t = AddTest (testList, "sans-serif",
    //             style_western_normal_16,
    //             S_UTF8,
    //             "\xe0\xb4\xb3\xe0\xb5\x8d\xe2\x80\x8d");
    // Disabled because these appear to vary
    //t->Expect ("macosx", "MalayalamMN", GLYPHS(360));
    //t->Expect ("win32", "Kartika", GLYPHS(332));
}
