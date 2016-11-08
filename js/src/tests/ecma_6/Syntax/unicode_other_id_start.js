/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// From PropList.txt (Unicode 9):
const otherIdStart = [
    // Enable the following lines when Bug 1282724 is fixed.
    // 0x1885,     // MONGOLIAN LETTER ALI GALI BALUDA, Gc=Mn
    // 0x1886,     // MONGOLIAN LETTER ALI GALI THREE BALUDA, Gc=Mn
    0x2118,     // SCRIPT CAPITAL P, Gc=Sm
    0x212E,     // ESTIMATED SYMBOL, Gc=So
    0x309B,     // KATAKANA-HIRAGANA VOICED SOUND MARK, Gc=Sk
    0x309C,     // KATAKANA-HIRAGANA SEMI-VOICED SOUND MARK, Gc=Sk
];

// Remove this list when we support Unicode 9 (Bug 1282724).
const otherIdStart_Unicode9 = [
    0x1885,     // MONGOLIAN LETTER ALI GALI BALUDA, Gc=Mn
    0x1886,     // MONGOLIAN LETTER ALI GALI THREE BALUDA, Gc=Mn
];

// Leading character in identifier.
for (let ident of [...otherIdStart, ...otherIdStart_Unicode9]) {
    eval(`
        let ${String.fromCodePoint(ident)} = 123;
        assertEq(${String.fromCodePoint(ident)}, 123);
    `);
    eval(`
        let \\u${ident.toString(16).padStart(4, "0")} = 123;
        assertEq(${String.fromCodePoint(ident)}, 123);
    `);
    eval(`
        let \\u{${ident.toString(16)}} = 123;
        assertEq(${String.fromCodePoint(ident)}, 123);
    `);
}

// Not leading character in identifier.
for (let ident of [...otherIdStart, ...otherIdStart_Unicode9]) {
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
