// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const languageTags = {
    // The preferred value of "hy-arevela" is "hy".
    "hy-arevela": "hy",
    "hy-Armn-arevela": "hy-Armn",
    "hy-AM-arevela": "hy-AM",
    "hy-arevela-fonipa": "hy-fonipa",
    "hy-fonipa-arevela": "hy-fonipa",

    // The preferred value of "hy-arevmda" is "hyw".
    "hy-arevmda": "hyw",
    "hy-Armn-arevmda": "hyw-Armn",
    "hy-AM-arevmda": "hyw-AM",
    "hy-arevmda-fonipa": "hyw-fonipa",
    "hy-fonipa-arevmda": "hyw-fonipa",

    // The preferred value of "ja-Latn-hepburn-heploc" is "ja-Latn-alalc97".
    "ja-Latn-hepburn-heploc": "ja-Latn-alalc97",
    "ja-Latn-JP-hepburn-heploc": "ja-Latn-JP-alalc97",

    // Ensure we don't emit "alalc97" when it is already present.
    "ja-Latn-alalc97-hepburn-heploc": "ja-Latn-alalc97",
    "ja-Latn-hepburn-alalc97-heploc": "ja-Latn-alalc97",
    "ja-Latn-hepburn-heploc-alalc97": "ja-Latn-alalc97",

    // Variants are reordered before canonicalizing, so "heploc-hepburn" is
    // reordered to "hepburn-heploc" which then can be canonicalized to "alalc97".
    "ja-Latn-heploc-hepburn": "ja-Latn-alalc97",
};

for (let [tag, canonical] of Object.entries(languageTags)) {
    assertEq(Intl.getCanonicalLocales(tag)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
