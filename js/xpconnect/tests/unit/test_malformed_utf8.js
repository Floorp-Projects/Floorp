/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// nsIPrefBranch.{getCharPref,setCharPref} uses Latin-1 string, and
// nsIPrefBranch.{getStringPref,setStringPref} uses UTF-8 string.
//
// Mixing them results in unexpected string, but it should perform lossy
// conversion, and not throw.

const gPrefs = Cc["@mozilla.org/preferences-service;1"].getService(Ci.nsIPrefBranch);

const tests = [
  // Latin-1 to Latin-1 and UTF-8 to UTF-8 should preserve the string.
  // Latin-1 to UTF-8 should replace invalid character with REPLACEMENT
  // CHARACTER.
  // UTF-8 to Latin1 should return the raw UTF-8 code units.

  // UTF-8 code units sequence without the last unit.
  //
  // input, Latin-1 to UTF-8, UTF-8 to Latin-1
  ["\xC2", "\uFFFD", "\xC3\x82"],
  ["\xDF", "\uFFFD", "\xC3\x9F"],
  ["\xE0\xA0", "\uFFFD", "\xC3\xA0\xC2\xA0"],
  ["\xF0\x90\x80", "\uFFFD", "\xC3\xB0\xC2\x90\xC2\x80"],

  // UTF-8 code units sequence with malformed last unit.
  //
  // input, Latin-1 to UTF-8, UTF-8 to Latin-1
  ["\xC2 ", "\uFFFD ", "\xC3\x82 "],
  ["\xDF ", "\uFFFD ", "\xC3\x9F "],
  ["\xE0\xA0 ", "\uFFFD ", "\xC3\xA0\xC2\xA0 "],
  ["\xF0\x90\x80 ", "\uFFFD ", "\xC3\xB0\xC2\x90\xC2\x80 "],

  // UTF-8 code units without the first unit.
  //
  // input, Latin-1 to UTF-8, UTF-8 to Latin-1
  ["\x80", "\uFFFD", "\xC2\x80"],
  ["\xBF", "\uFFFD", "\xC2\xBF"],
  ["\xA0\x80", "\uFFFD\uFFFD", "\xC2\xA0\xC2\x80"],
  ["\x8F\x80\x80", "\uFFFD\uFFFD\uFFFD", "\xC2\x8F\xC2\x80\xC2\x80"],
];

add_task(function testLatin1ToLatin1() {
  for (const [input, ] of tests) {
    gPrefs.setCharPref("test.malformed_utf8_data", input);
    const result = gPrefs.getCharPref("test.malformed_utf8_data");
    Assert.equal(result, input);
  }
});

add_task(function testLatin1ToUTF8() {
  for (const [input, expected] of tests) {
    gPrefs.setCharPref("test.malformed_utf8_data", input);
    const result = gPrefs.getStringPref("test.malformed_utf8_data");
    Assert.equal(result, expected);
  }
});

add_task(function testUTF8ToLatin1() {
  for (const [input, , expected] of tests) {
    gPrefs.setStringPref("test.malformed_utf8_data", input);
    const result = gPrefs.getCharPref("test.malformed_utf8_data");
    Assert.equal(result, expected);
  }
});

add_task(function testUTF8ToUTF8() {
  for (const [input, ] of tests) {
    gPrefs.setStringPref("test.malformed_utf8_data", input);
    const result = gPrefs.getStringPref("test.malformed_utf8_data");
    Assert.equal(result, input);
  }
});
