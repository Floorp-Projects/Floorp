/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Corporation code.
 *
 * The Initial Developer of the Original Code is Mozilla Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
SetupTests()
{
    TestEntry *t;

    /* some common styles */
    gfxFontStyle style_western_normal_16 (FONT_STYLE_NORMAL,
                                          FONT_VARIANT_NORMAL,
                                          400,
                                          FONT_DECORATION_NONE,
                                          16.0,
                                          nsDependentCString("x-western"),
                                          0.0,
                                          PR_FALSE, PR_FALSE);

    gfxFontStyle style_western_bold_16 (FONT_STYLE_NORMAL,
                                        FONT_VARIANT_NORMAL,
                                        700,
                                        FONT_DECORATION_NONE,
                                        16.0,
                                        nsDependentCString("x-western"),
                                        0.0,
                                        PR_FALSE, PR_FALSE);

    /* Test 0 */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 "ABCD");

    t->Expect ("win32", "Arial", GLYPHS(36, 37, 38, 39));
    t->Expect ("macosx", "Helvetica", GLYPHS(36, 37, 38, 39));
    t->Expect ("gtk2-pango", "Albany AMT", GLYPHS(36, 37, 38, 39));

    /* Test 1 */
    t = AddTest ("verdana,sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 "foo\xe2\x80\x91""bar");

    t->Expect ("win32", "Verdana", GLYPHS(73, 82, 82));
    t->Expect ("win32", "MS UI Gothic", GLYPHS(19401));
    t->Expect ("win32", "Verdana", GLYPHS(69, 68, 85));

    t->Expect ("macosx", "Verdana", GLYPHS(73, 82, 82));
    t->Expect ("macosx", "Helvetica", GLYPHS(587));
    t->Expect ("macosx", "Verdana", GLYPHS(69, 68, 85));

    /* Test 2 */
    t = AddTest ("sans-serif",
                 style_western_bold_16,
                 S_ASCII,
                 "ABCD");

    t->Expect ("win32", "Arial:700", GLYPHS(36, 37, 38, 39));
    t->Expect ("macosx", "Helvetica-Bold", GLYPHS(36, 37, 38, 39));
    t->Expect ("gtk2-pango", "Albany AMT Bold", GLYPHS(36, 37, 38, 39));

    /* Test 3: RTL Arabic with a ligature and leading and trailing whitespace */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 " \xd8\xaa\xd9\x85 ");
    t->SetRTL();
    t->Expect ("macosx", "Helvetica", GLYPHS(3));
    t->Expect ("macosx", "AlBayan", GLYPHS(47));
    t->Expect ("macosx", "Helvetica", GLYPHS(3));

    /* Test 4: LTR Arabic with leading and trailing whitespace */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 " \xd9\x85\xd8\xaa ");
    t->Expect ("macosx", "Helvetica", GLYPHS(3));
    t->Expect ("macosx", "AlBayan", GLYPHS(2, 47));
    t->Expect ("macosx", "Helvetica", GLYPHS(3));

    /* Test 5: RTL ASCII with leading whitespace */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 " ab");
    t->SetRTL();
    t->Expect ("macosx", "Helvetica", GLYPHS(3, 68, 69));
    t->Expect ("win32", "Arial", GLYPHS(3, 68, 69));
    t->Expect ("gtk2-pango", "Albany AMT", GLYPHS(3, 68, 69));

    /* Test 6: RTL ASCII with trailing whitespace */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 "ab ");
    t->SetRTL();
    t->Expect ("macosx", "Helvetica", GLYPHS(68, 69, 3));
    t->Expect ("win32", "Arial", GLYPHS(68, 69, 3));
    t->Expect ("gtk2-pango", "Albany AMT", GLYPHS(68, 69, 3));

    /* Test 7: Simple ASCII ligature */
    /* Do we have a Windows font with ligatures? Can we use DejaVu Sans? */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_ASCII,
                 "fi");
    t->Expect ("macosx", "Helvetica", GLYPHS(192));

    /* Test 8: DEVANAGARI VOWEL I reordering */
    /* The glyph for DEVANAGARI VOWEL I 2367 (101) is displayed before the glyph for 2361 (99) */
    t = AddTest ("sans-serif",
                 style_western_normal_16,
                 S_UTF8,
                 "\xe0\xa4\x9a\xe0\xa4\xbe\xe0\xa4\xb9\xe0\xa4\xbf\xe0\xa4\x8f"); // 2330 2366 2361 2367 2319
    t->Expect ("macosx", "DevanagariMT", GLYPHS(71, 100, 101, 99, 60));
    t->Expect ("win32", "Mangal", GLYPHS(133, 545, 465, 161, 102));
}
