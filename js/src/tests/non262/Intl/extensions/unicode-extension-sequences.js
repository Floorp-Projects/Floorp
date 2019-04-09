// |reftest| skip-if(!this.getSelfHostedValue)

const startOfUnicodeExtensions = getSelfHostedValue("startOfUnicodeExtensions");
const endOfUnicodeExtensions = getSelfHostedValue("endOfUnicodeExtensions");

const testcases = [
    // Language tag without Unicode extension.
    { locale: "en", start: -1, end: 0 },
    { locale: "en-Latn", start: -1, end: 0 },
    { locale: "en-x-y", start: -1, end: 0 },
    { locale: "en-x-yz", start: -1, end: 0 },
    { locale: "en-x-u-kf", start: -1, end: 0 },

    // Unicode extension sequence starts with key subtag.
    // - no suceeding key or type subtags.
    { locale: "en-u-ab", start: 2, end: 7 },
    { locale: "en-u-ab-x-y", start: 2, end: 7 },
    { locale: "en-u-ab-x-yz", start: 2, end: 7 },
    { locale: "en-u-ab-x-u-kn", start: 2, end: 7 },
    // - followed by key subtag.
    { locale: "en-u-ab-cd", start: 2, end: 10 },
    { locale: "en-u-ab-cd-x-y", start: 2, end: 10 },
    { locale: "en-u-ab-cd-x-yz", start: 2, end: 10 },
    { locale: "en-u-ab-cd-x-u-kn", start: 2, end: 10 },
    // - followed by type subtag.
    { locale: "en-u-ab-cdef", start: 2, end: 12 },
    { locale: "en-u-ab-cdef-x-y", start: 2, end: 12 },
    { locale: "en-u-ab-cdef-x-yz", start: 2, end: 12 },
    { locale: "en-u-ab-cdef-x-y-u-kn", start: 2, end: 12 },

    // Unicode extension sequence starts with attribute subtag.
    // - no suceeding attribute or key subtags.
    { locale: "en-u-abc", start: 2, end: 8 },
    { locale: "en-u-abc-x-y", start: 2, end: 8 },
    { locale: "en-u-abc-x-yz", start: 2, end: 8 },
    { locale: "en-u-abc-x-y-u-kn", start: 2, end: 8 },
    // - followed by attribute subtag.
    { locale: "en-u-abc-def", start: 2, end: 12 },
    { locale: "en-u-abc-def-x-y", start: 2, end: 12 },
    { locale: "en-u-abc-def-x-yz", start: 2, end: 12 },
    { locale: "en-u-abc-def-x-y-u-kn", start: 2, end: 12 },
    // - followed by key subtag.
    { locale: "en-u-abc-de", start: 2, end: 11 },
    { locale: "en-u-abc-de-x-y", start: 2, end: 11 },
    { locale: "en-u-abc-de-x-yz", start: 2, end: 11 },
    { locale: "en-u-abc-de-x-y-u-kn", start: 2, end: 11 },
    // - followed by two key subtags.
    { locale: "en-u-abc-de-fg", start: 2, end: 14 },
    { locale: "en-u-abc-de-fg-x-y", start: 2, end: 14 },
    { locale: "en-u-abc-de-fg-x-yz", start: 2, end: 14 },
    { locale: "en-u-abc-de-fg-x-y-u-kn", start: 2, end: 14 },
    // - followed by key and type subtag.
    { locale: "en-u-abc-de-fgh", start: 2, end: 15 },
    { locale: "en-u-abc-de-fgh-x-y", start: 2, end: 15 },
    { locale: "en-u-abc-de-fgh-x-yz", start: 2, end: 15 },
    { locale: "en-u-abc-de-fgh-x-y-u-kn", start: 2, end: 15 },

    // Also test when the Unicode extension doesn't start at index 2.
    { locale: "en-Latn-u-kf", start: 7, end: 12 },
    { locale: "und-u-kf", start: 3, end: 8 },
];

for (const {locale, start, end} of testcases) {
    // Ensure the input is a valid language tag.
    assertEqArray(Intl.getCanonicalLocales(locale), [locale]);

    assertEq(startOfUnicodeExtensions(locale), start);

    if (start >= 0)
        assertEq(endOfUnicodeExtensions(locale, start), end);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
