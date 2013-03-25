// |reftest| skip-if(!this.hasOwnProperty("Intl"))
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// Tests the compare function with a diverse set of locales and options.

var input = [
    "Argentina",
    "Oerlikon",
    "Offenbach",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "la France",
    "¡viva España!",
    "Österreich",
    "中国",
    "日本",
    "한국",
];

var collator, expected;

function assertEqualArray(actual, expected, collator) {
    var description = JSON.stringify(collator.resolvedOptions());
    assertEq(actual.length, expected.length, "array length, " + description);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i], expected[i], "element " + i + ", " + description);
    }
}


// Locale en-US; default options.
collator = new Intl.Collator("en-US");
expected = [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Österreich",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "한국",
    "中国",
    "日本",
];
assertEqualArray(input.sort(collator.compare), expected, collator);

// Locale sv-SE; default options.
// Swedish treats "Ö" as a separate character, which sorts after "Z".
collator = new Intl.Collator("sv-SE");
expected = [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "Österreich",
    "한국",
    "中国",
    "日本",
];
assertEqualArray(input.sort(collator.compare), expected, collator);

// Locale sv-SE; ignore punctuation.
collator = new Intl.Collator("sv-SE", {ignorePunctuation: true});
expected = [
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Sverige",
    "Vaticano",
    "¡viva España!",
    "Zimbabwe",
    "Österreich",
    "한국",
    "中国",
    "日本",
];
assertEqualArray(input.sort(collator.compare), expected, collator);

// Locale de-DE; default options.
// In German standard sorting, umlauted characters are treated as variants
// of their base characters: ä ≅ a, ö ≅ o, ü ≅ u.
collator = new Intl.Collator("de-DE");
expected = [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Österreich",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "한국",
    "中国",
    "日本",
];
assertEqualArray(input.sort(collator.compare), expected, collator);

// Locale de-DE; phonebook sort order.
// In German phonebook sorting, umlauted characters are expanded to two-vowel
// sequences: ä → ae, ö → oe, ü → ue.
collator = new Intl.Collator("de-DE-u-co-phonebk");
expected = [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Österreich",
    "Offenbach",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "한국",
    "中国",
    "日本",
];
assertEqualArray(input.sort(collator.compare), expected, collator);

reportCompare(0, 0, 'ok');
