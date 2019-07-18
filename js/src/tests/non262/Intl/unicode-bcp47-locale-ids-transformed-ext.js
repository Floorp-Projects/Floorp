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

// Canonicalisation also applies for the transformation extension. But also
// see <https://github.com/tc39/ecma402/issues/330>.
const valid = [
    {locale: "en-t-en", canonical: "en-t-en"},
    {locale: "en-t-en-latn", canonical: "en-t-en-latn"},
    {locale: "en-t-en-ca", canonical: "en-t-en-ca"},
    {locale: "en-t-en-latn-ca", canonical: "en-t-en-latn-ca"},
    {locale: "en-t-en-emodeng", canonical: "en-t-en-emodeng"},
    {locale: "en-t-en-latn-emodeng", canonical: "en-t-en-latn-emodeng"},
    {locale: "en-t-en-latn-ca-emodeng", canonical: "en-t-en-latn-ca-emodeng"},
    {locale: "sl-t-sl-rozaj-biske-1994", canonical: "sl-t-sl-rozaj-biske-1994"},
    {locale: "DE-T-M0-DIN-K0-QWERTZ", canonical: "de-t-k0-qwertz-m0-din"},
    {locale: "en-t-m0-true", canonical: "en-t-m0-true"},
];

for (let locale of invalid) {
    assertThrowsInstanceOf(() => Intl.getCanonicalLocales(locale), RangeError);
}

for (let {locale, canonical} of valid) {
    assertEq(Intl.getCanonicalLocales(locale)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
