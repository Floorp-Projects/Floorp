// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Per UTS 35, computing the canonical form for Unicode BCP 47 locale identifiers
// includes replacing deprecated variant mappings. The other UTS 35 canonicalisation
// algorithm ("BCP 47 Language Tag to Unicode BCP 47 Locale Identifier") doesn't
// support deprecated variant mappings.
// https://github.com/tc39/ecma402/issues/330

const languageTags = {
    // The preferred value of "hy-arevela" is "hy" per CLDR.
    "hy-arevela": "hy",
    "hy-Armn-arevela": "hy-Armn",
    "hy-AM-arevela": "hy-AM",
    "hy-arevela-fonipa": "hy-fonipa",
    "hy-fonipa-arevela": "hy-fonipa",

    // The preferred value of "hy-arevmda" is "hyw" per CLDR.
    "hy-arevmda": "hyw",
    "hy-Armn-arevmda": "hyw-Armn",
    "hy-AM-arevmda": "hyw-AM",
    "hy-arevmda-fonipa": "hyw-fonipa",
    "hy-fonipa-arevmda": "hyw-fonipa",

    // The preferred value of "ja-Latn-hepburn-heploc" is "ja-Latn-alalc97" per CLDR and
    // IANA.
    "ja-Latn-hepburn-heploc": "ja-Latn-alalc97",
    "ja-Latn-JP-hepburn-heploc": "ja-Latn-JP-alalc97",

    // Variant subtag replacements not present in IANA.
    "sv-aaland": "sv-AX",
    "el-polytoni": "el-polyton",

    // Additional cases when more variant subtags are present.

    // 1. The preferred variant is already present.
    "ja-Latn-alalc97-hepburn-heploc": "ja-Latn-alalc97",
    "ja-Latn-hepburn-alalc97-heploc": "ja-Latn-alalc97",
    "ja-Latn-hepburn-heploc-alalc97": "ja-Latn-alalc97",

    // 2. The variant subtags aren't in the expected order per IANA. (CLDR doesn't care
    //    about the order of variant subtags.)
    "ja-Latn-heploc-hepburn": "ja-Latn-alalc97",

    // 3. IANA expects both variant subtags to be present, CLDR also accepts single "heploc".
    "ja-Latn-heploc": "ja-Latn-alalc97",

    // 4. Test for cases when the same variant subtag position needs to be checked more
    //    than once when replacing deprecated variant subtags.
    "ja-Latn-aaland-heploc": "ja-Latn-AX-alalc97",
    "ja-Latn-heploc-polytoni": "ja-Latn-alalc97-polyton",
};

for (let [tag, canonical] of Object.entries(languageTags)) {
    assertEq(Intl.getCanonicalLocales(tag)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
