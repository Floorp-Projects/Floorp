/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// White space must not be determined by truncating char32_t to char16_t.

assertThrowsInstanceOf(() => eval("\u{40008}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{40009}"), SyntaxError); // U+0009 CHARACTER TABULATION
assertThrowsInstanceOf(() => eval("\u{4000A}"), SyntaxError); // U+000A LINE FEED
assertThrowsInstanceOf(() => eval("\u{4000B}"), SyntaxError); // U+000B LINE TABULATION
assertThrowsInstanceOf(() => eval("\u{4000C}"), SyntaxError); // U+000C FORM FEED
assertThrowsInstanceOf(() => eval("\u{4000D}"), SyntaxError); // U+000D CARRIAGE RETURN
assertThrowsInstanceOf(() => eval("\u{4000E}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{4001F}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{40020}"), SyntaxError); // U+0020 SPACE
assertThrowsInstanceOf(() => eval("\u{40021}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{4009F}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{400A0}"), SyntaxError); // U+00A0 NO-BREAK SPACE
assertThrowsInstanceOf(() => eval("\u{400A1}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{4167F}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{41680}"), SyntaxError); // U+1680 OGHAM SPACE MARK
assertThrowsInstanceOf(() => eval("\u{41681}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{41FFF}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{42000}"), SyntaxError); // U+2000 EN QUAD
assertThrowsInstanceOf(() => eval("\u{42001}"), SyntaxError); // U+2001 EM QUAD
assertThrowsInstanceOf(() => eval("\u{42002}"), SyntaxError); // U+2002 EN SPACE
assertThrowsInstanceOf(() => eval("\u{42003}"), SyntaxError); // U+2003 EM SPACE
assertThrowsInstanceOf(() => eval("\u{42004}"), SyntaxError); // U+2004 THREE-PER-EM SPACE
assertThrowsInstanceOf(() => eval("\u{42005}"), SyntaxError); // U+2005 FOUR-PER-EM SPACE
assertThrowsInstanceOf(() => eval("\u{42006}"), SyntaxError); // U+2006 SIX-PER-EM SPACE
assertThrowsInstanceOf(() => eval("\u{42007}"), SyntaxError); // U+2007 FIGURE SPACE
assertThrowsInstanceOf(() => eval("\u{42008}"), SyntaxError); // U+2008 PUNCTUATION SPACE
assertThrowsInstanceOf(() => eval("\u{42009}"), SyntaxError); // U+2009 THIN SPACE
assertThrowsInstanceOf(() => eval("\u{4200A}"), SyntaxError); // U+200A HAIR SPACE
assertThrowsInstanceOf(() => eval("\u{4200B}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{42027}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{42028}"), SyntaxError); // U+2028 LINE SEPARATOR
assertThrowsInstanceOf(() => eval("\u{42029}"), SyntaxError); // U+2028 PARAGRAPH SEPARATOR
assertThrowsInstanceOf(() => eval("\u{4202A}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{4202E}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{4202F}"), SyntaxError); // U+202F NARROW NO-BREAK SPACE
assertThrowsInstanceOf(() => eval("\u{42030}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{4205E}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{4205F}"), SyntaxError); // U+205F MEDIUM MATHEMATICAL SPACE
assertThrowsInstanceOf(() => eval("\u{42060}"), SyntaxError);

assertThrowsInstanceOf(() => eval("\u{42FFF}"), SyntaxError);
assertThrowsInstanceOf(() => eval("\u{43000}"), SyntaxError); // U+3000 IDEOGRAPHIC SPACE
assertThrowsInstanceOf(() => eval("\u{43001}"), SyntaxError);

if (typeof reportCompare === "function")
  reportCompare(true, true);
