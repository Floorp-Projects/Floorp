/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var mongolian_vowel_separator = "\u180e";

assertEq(/^\s+$/.exec(mongolian_vowel_separator) === null, true);
assertEq(/^[\s]+$/.exec(mongolian_vowel_separator) === null, true);
assertEq(/^[^\s]+$/.exec(mongolian_vowel_separator) !== null, true);

assertEq(/^\S+$/.exec(mongolian_vowel_separator) !== null, true);
assertEq(/^[\S]+$/.exec(mongolian_vowel_separator) !== null, true);
assertEq(/^[^\S]+$/.exec(mongolian_vowel_separator) === null, true);

// Also test with Unicode RegExps.
assertEq(/^\s+$/u.exec(mongolian_vowel_separator) === null, true);
assertEq(/^[\s]+$/u.exec(mongolian_vowel_separator) === null, true);
assertEq(/^[^\s]+$/u.exec(mongolian_vowel_separator) !== null, true);

assertEq(/^\S+$/u.exec(mongolian_vowel_separator) !== null, true);
assertEq(/^[\S]+$/u.exec(mongolian_vowel_separator) !== null, true);
assertEq(/^[^\S]+$/u.exec(mongolian_vowel_separator) === null, true);

if (typeof reportCompare === "function")
    reportCompare(true, true);
