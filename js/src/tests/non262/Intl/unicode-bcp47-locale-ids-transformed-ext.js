// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const invalid = [
    // empty
    "en-t",
    "en-t-a",
    "en-t-x",
    "en-t-0",

    // incomplete
    "en-t-",
    "en-t-en-",
    "en-t-0x-",

    // tlang: unicode_language_subtag must be 2-3 or 5-8 characters and mustn't
    // contain extlang subtags.
    "en-t-root",
    "en-t-abcdefghi",
    "en-t-ar-aao",

    // tlang: unicode_script_subtag must be 4 alphabetical characters, can't
    // be repeated.
    "en-t-en-lat0",
    "en-t-en-latn-latn",

    // tlang: unicode_region_subtag must either be 2 alpha characters or a three
    // digit code.
    "en-t-en-0",
    "en-t-en-00",
    "en-t-en-0x",
    "en-t-en-x0",
    "en-t-en-latn-0",
    "en-t-en-latn-00",
    "en-t-en-latn-xyz",

    // tlang: unicode_variant_subtag is either 5-8 alphanum characters or 4
    // characters starting with a digit.
    "en-t-en-abcdefghi",
    "en-t-en-latn-gb-ab",
    "en-t-en-latn-gb-abc",
    "en-t-en-latn-gb-abcd",
    "en-t-en-latn-gb-abcdefghi",
];

// Note: Case normalisation in the transformation extension is currently not
// implemented, see also <https://github.com/tc39/ecma402/issues/330>.
const valid = [
    "en-t-en",
    "en-t-en-latn",
    "en-t-en-ca",
    "en-t-en-latn-ca",
    "en-t-en-emodeng",
    "en-t-en-latn-emodeng",
    "en-t-en-latn-ca-emodeng",
];

for (let locale of invalid) {
    assertThrowsInstanceOf(() => Intl.getCanonicalLocales(locale), RangeError);
}

for (let locale of valid) {
    assertEq(Intl.getCanonicalLocales(locale)[0], locale);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
