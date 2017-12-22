/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// U+2E2F (VERTICAL TILDE) is in Gc=Lm, but also in [:Pattern_Syntax:].
// http://www.unicode.org/reports/tr31/
const verticalTilde = 0x2E2F;

// Leading character in identifier.
assertThrowsInstanceOf(() => eval(`${String.fromCodePoint(verticalTilde)}`), SyntaxError);
assertThrowsInstanceOf(() => eval(`\\u${verticalTilde.toString(16).padStart(4, "0")}`), SyntaxError);
assertThrowsInstanceOf(() => eval(`\\u{${verticalTilde.toString(16)}}`), SyntaxError);

// Not leading character in identifier.
assertThrowsInstanceOf(() => eval(`A${String.fromCodePoint(verticalTilde)}`), SyntaxError);
assertThrowsInstanceOf(() => eval(`A\\u${verticalTilde.toString(16).padStart(4, "0")}`), SyntaxError);
assertThrowsInstanceOf(() => eval(`A\\u{${verticalTilde.toString(16)}}`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
