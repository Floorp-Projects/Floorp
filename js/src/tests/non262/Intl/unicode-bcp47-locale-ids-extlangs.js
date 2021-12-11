// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Unicode BCP 47 locale identifiers don't support extlang subtags.
const invalid = [
    // Two letter language code followed by extlang subtags.
    "en-abc",
    "en-abc-def",
    "en-abc-def-ghi",

    // Three letter language code followed by extlang subtags.
    "und-abc",
    "und-abc-def",
    "und-abc-def-ghi",
];

for (let locale of invalid) {
    assertThrowsInstanceOf(() => Intl.getCanonicalLocales(locale), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
