// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// UTS 35, "BCP 47 Language Tag to Unicode BCP 47 Locale Identifier" doesn't
// support deprecated variant mappings.
// https://github.com/tc39/ecma402/issues/330

const languageTags = {
    // The preferred value of "hy-arevela" is "hy" per IANA.
    "hy-arevela": "hy-arevela",
    "hy-Armn-arevela": "hy-Armn-arevela",
    "hy-AM-arevela": "hy-AM-arevela",
    "hy-arevela-fonipa": "hy-arevela-fonipa",
    "hy-fonipa-arevela": "hy-arevela-fonipa",

    // The preferred value of "hy-arevmda" is "hyw" per IANA.
    "hy-arevmda": "hy-arevmda",
    "hy-Armn-arevmda": "hy-Armn-arevmda",
    "hy-AM-arevmda": "hy-AM-arevmda",
    "hy-arevmda-fonipa": "hy-arevmda-fonipa",
    "hy-fonipa-arevmda": "hy-arevmda-fonipa",

    // The preferred value of "ja-Latn-hepburn-heploc" is "ja-Latn-alalc97" per IANA.
    "ja-Latn-hepburn-heploc": "ja-Latn-hepburn-heploc",
    "ja-Latn-JP-hepburn-heploc": "ja-Latn-JP-hepburn-heploc",

    // Additional cases when more variant subtags are present.
    "ja-Latn-alalc97-hepburn-heploc": "ja-Latn-alalc97-hepburn-heploc",
    "ja-Latn-hepburn-alalc97-heploc": "ja-Latn-alalc97-hepburn-heploc",
    "ja-Latn-hepburn-heploc-alalc97": "ja-Latn-alalc97-hepburn-heploc",
    "ja-Latn-heploc-hepburn": "ja-Latn-hepburn-heploc",
};

for (let [tag, canonical] of Object.entries(languageTags)) {
    assertEq(Intl.getCanonicalLocales(tag)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
