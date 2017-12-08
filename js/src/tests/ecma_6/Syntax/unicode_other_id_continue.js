/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// From PropList.txt (Unicode 9):
const otherIdContinue = [
    0x00B7,     // MIDDLE DOT, Gc=Po
    0x0387,     // GREEK ANO TELEIA, Gc=Po
    0x1369,     // ETHIOPIC DIGIT ONE, Gc=No
    0x136A,     // ETHIOPIC DIGIT TWO, Gc=No
    0x136B,     // ETHIOPIC DIGIT THREE, Gc=No
    0x136C,     // ETHIOPIC DIGIT FOUR, Gc=No
    0x136D,     // ETHIOPIC DIGIT FIVE, Gc=No
    0x136E,     // ETHIOPIC DIGIT SIX, Gc=No
    0x136F,     // ETHIOPIC DIGIT SEVEN, Gc=No
    0x1370,     // ETHIOPIC DIGIT EIGHT, Gc=No
    0x1371,     // ETHIOPIC DIGIT NINE, Gc=No
    0x19DA,     // NEW TAI LUE THAM DIGIT ONE, Gc=No
];

// Leading character in identifier.
for (let ident of [...otherIdContinue]) {
    assertThrowsInstanceOf(() => eval(`${String.fromCodePoint(ident)}`), SyntaxError);
    assertThrowsInstanceOf(() => eval(`\\u${ident.toString(16).padStart(4, "0")}`), SyntaxError);
    assertThrowsInstanceOf(() => eval(`\\u{${ident.toString(16)}}`), SyntaxError);
}

// Not leading character in identifier.
for (let ident of [...otherIdContinue]) {
    eval(`
        let A${String.fromCodePoint(ident)} = 123;
        assertEq(${String.fromCodePoint(0x41, ident)}, 123);
    `);
    eval(`
        let A\\u${ident.toString(16).padStart(4, "0")} = 123;
        assertEq(${String.fromCodePoint(0x41, ident)}, 123);
    `);
    eval(`
        let A\\u{${ident.toString(16)}} = 123;
        assertEq(${String.fromCodePoint(0x41, ident)}, 123);
    `);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
