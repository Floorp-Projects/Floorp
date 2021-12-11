// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Unicode BCP 47 locale identifiers don't support irregular legacy tags.
var irregularLegacy = [
    "en-gb-oed",
    "i-ami",
    "i-bnn",
    "i-default",
    "i-enochian",
    "i-hak",
    "i-klingon",
    "i-lux",
    "i-mingo",
    "i-navajo",
    "i-pwn",
    "i-tao",
    "i-tay",
    "i-tsu",
    "sgn-be-fr",
    "sgn-be-nl",
    "sgn-ch-de",
];

// Unicode BCP 47 locale identifiers don't support regular legacy tags
// which contain an extlang-like subtag.
var regularLegacyWithExtlangLike = [
    "no-bok",
    "no-nyn",
    "zh-min",
    "zh-min-nan",
];

// Unicode BCP 47 locale identifiers do support regular legacy tags
// which contain a variant-like subtag.
var regularLegacyWithVariantLike = {
    "art-lojban": "jbo",
    "cel-gaulish": "xtg",
    "zh-guoyu": "zh",
    "zh-hakka": "hak",
    "zh-xiang": "hsn",
};

for (let locale of [...irregularLegacy, ...regularLegacyWithExtlangLike]) {
    assertThrowsInstanceOf(() => Intl.getCanonicalLocales(locale), RangeError);
}

for (let [locale, canonical] of Object.entries(regularLegacyWithVariantLike)) {
    assertEq(Intl.getCanonicalLocales(locale)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
