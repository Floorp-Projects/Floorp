/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Simple cases, not using eval.
{
    let \u{61} = 123;
    assertEq(a, 123);

    let \u{6A} = 123;
    assertEq(j, 123);

    let a\u{62} = 456;
    assertEq(ab, 456);

    let \u{63}\u{6b} = 789;
    assertEq(ck, 789);
}

const leadingZeros = [0, 1, 2, 3, 4, 100].map(c => "0".repeat(c));


// From DerivedCoreProperties.txt (Unicode 9):
// Derived Property: ID_Start
//  Characters that can start an identifier.
//  Generated from:
//      Lu + Ll + Lt + Lm + Lo + Nl
//    + Other_ID_Start
//    - Pattern_Syntax
//    - Pattern_White_Space
const idStart = [
    0x0041,     // LATIN CAPITAL LETTER A, Gc=Lu
    0x006A,     // LATIN SMALL LETTER J, Gc=Ll
    0x00C9,     // LATIN CAPITAL LETTER E WITH ACUTE, Gc=Lu
    0x00FF,     // LATIN SMALL LETTER Y WITH DIAERESIS, Gc=Ll
    0x01C5,     // LATIN CAPITAL LETTER D WITH SMALL LETTER Z WITH CARON, Gc=Lt
    0x0294,     // LATIN LETTER GLOTTAL STOP, Gc=Lo
    0x037A,     // GREEK YPOGEGRAMMENI, Gc=Lm
    0x16EE,     // RUNIC ARLAUG SYMBOL, Gc=Nl
    0xFF70,     // HALFWIDTH KATAKANA-HIRAGANA PROLONGED SOUND MARK, Gc=Lm
];

const idStartSupplemental = [
    0x10140,    // GREEK ACROPHONIC ATTIC ONE QUARTER, Gc=Nl
    0x10300,    // OLD ITALIC LETTER A, Gc=Lo
    0x10400,    // DESERET CAPITAL LETTER LONG I, Gc=Lu
    0x10430,    // DESERET SMALL LETTER SHORT A, Gc=Ll
    0x16B40,    // PAHAWH HMONG SIGN VOS SEEV, Gc=Lm
];

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

// From DerivedCoreProperties.txt (Unicode 9):
// Derived Property: ID_Continue
//  Characters that can continue an identifier.
//  Generated from:
//      ID_Start
//    + Mn + Mc + Nd + Pc
//    + Other_ID_Continue
//    - Pattern_Syntax
//    - Pattern_White_Space
const idContinue = [
    0x0030,     // DIGIT ZERO, Gc=Nd
    0x0300,     // COMBINING GRAVE ACCENT, Gc=Mn
    0x0660,     // ARABIC-INDIC DIGIT ZERO, Gc=Nd
    0x0903,     // DEVANAGARI SIGN VISARGA, Gc=Mc
    0xFF10,     // FULLWIDTH DIGIT ZERO, Gc=Nd
    0xFF3F,     // FULLWIDTH LOW LINE, Gc=Pc
];

const idContinueSupplemental = [
    0x101FD,    // PHAISTOS DISC SIGN COMBINING OBLIQUE STROKE, Gc=Mn
    0x104A0,    // OSMANYA DIGIT ZERO, Gc=Nd
    0x11000,    // BRAHMI SIGN CANDRABINDU, Gc=Mc
];

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

for (let ident of [...idStart, ...otherIdStart_Unicode9]) {
    for (let count of leadingZeros) {
        let zeros = "0".repeat(count);
        eval(`
            let \\u{${zeros}${ident.toString(16)}} = 123;
            assertEq(${String.fromCodePoint(ident)}, 123);
        `);
    }
}

// Move this to the loop above when Bug 917436 is fixed.
for (let ident of [...idStartSupplemental, ...otherIdStart]) {
    for (let zeros of leadingZeros) {
        assertThrowsInstanceOf(() => eval(`\\u{${zeros}${ident.toString(16)}}`), SyntaxError);
    }
}

for (let ident of [...idContinue, ...idContinueSupplemental, ...otherIdContinue]) {
    for (let zeros of leadingZeros) {
        assertThrowsInstanceOf(() => eval(`\\u{${zeros}${ident.toString(16)}}`), SyntaxError);
    }
}

for (let ident of [...idStart, ...otherIdStart_Unicode9, ...idContinue]) {
    for (let zeros of leadingZeros) {
        eval(`
            let A\\u{${zeros}${ident.toString(16)}} = 123;
            assertEq(${String.fromCodePoint(0x41, ident)}, 123);
        `);
    }
}

// Move this to the loop above when Bug 917436 is fixed.
for (let ident of [...idStartSupplemental, ...otherIdStart, ...idContinueSupplemental, ...otherIdContinue]) {
    for (let zeros of leadingZeros) {
        assertThrowsInstanceOf(() => eval(`\\u{${zeros}${ident.toString(16)}}`), SyntaxError);
    }
}


const notIdentifiers = [
    0x0000,     // NULL, Gc=Cc
    0x000A,     // LINE FEED (LF), Gc=Cc
    0x005E,     // CIRCUMFLEX ACCENT, Gc=Sk
    0x00B1,     // PLUS-MINUS SIGN, Gc=Sm
    0xFF61,     // HALFWIDTH IDEOGRAPHIC FULL STOP, Gc=Po
    0x10061,    // Not assigned.
    0x10100,    // AEGEAN WORD SEPARATOR LINE, Gc=Po
    0x100061,   // <Plane 16 Private Use>, Gc=Co
];

for (let ident of notIdentifiers) {
    for (let zeros of leadingZeros) {
        assertThrowsInstanceOf(() => eval(`\\u{${zeros}${ident.toString(16)}}`), SyntaxError);
    }
}


const incompleteEscapes = [
    "\\u{",
    "\\u{6",
    "\\u{61",
    "\\u{061",
    "\\u{0061",
    "\\u{00061",
    "\\u{000061",
    "\\u{0000061",

    "\\u}",
];
for (let invalid of incompleteEscapes) {
    // Ends with EOF.
    assertThrowsInstanceOf(() => eval(invalid), SyntaxError);

    // Ends with EOL.
    assertThrowsInstanceOf(() => eval(invalid + "\n"), SyntaxError);

    // Ends with space.
    assertThrowsInstanceOf(() => eval(invalid + " "), SyntaxError);
}


const invalidEscapes = [
    // Empty escape.
    "",

    // Not hexadecimal characters.
    "\0",
    "G",
    "Z",
    "\uFFFF",
    "\uDBFF\uDFFF",

    // Has space characters.
    " 61",
    "61 ",

    // Has newline characters.
    "\n61",
    "61\n",

    // Exceeds 0x10FFFF, six characters.
    "110000",
    "110001",
    "fffffe",
    "ffffff",

    // Exceeds 0x10FFFF, more than six characters.
    "10ffff0",
    "10ffffabcdef",
];

for (let invalid of invalidEscapes) {
    for (let zeros of leadingZeros) {
        assertThrowsInstanceOf(() => eval(`\\u{${zeros}${invalid}}`), SyntaxError);
        assertThrowsInstanceOf(() => eval(`var \\u{${zeros}${invalid}}`), SyntaxError);
    }
}


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
