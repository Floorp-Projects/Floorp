// |reftest| skip-if(!this.hasOwnProperty("Intl"))

// Unicode BCP 47 locale identifiers don't support irregular grandfathered tags.
var irregularGrandfathered = [
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

// Unicode BCP 47 locale identifiers don't support regular grandfathered tags
// which contain an extlang-like subtag.
var regularGrandfatheredWithExtlangLike = [
    "no-bok",
    "no-nyn",
    "zh-min",
    "zh-min-nan",
];

// Unicode BCP 47 locale identifiers do support regular grandfathered tags
// which contain a variant-like subtag.
var regularGrandfatheredWithVariantLike = {
    "art-lojban": "jbo",
    "cel-gaulish": "cel-gaulish",
    "zh-guoyu": "cmn",
    "zh-hakka": "hak",
    "zh-xiang": "hsn",
};

for (let locale of [...irregularGrandfathered, ...regularGrandfatheredWithExtlangLike]) {
    assertThrowsInstanceOf(() => Intl.getCanonicalLocales(locale), RangeError);
}

for (let [locale, canonical] of Object.entries(regularGrandfatheredWithVariantLike)) {
    assertEq(Intl.getCanonicalLocales(locale)[0], canonical);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
