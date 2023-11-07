// |reftest| skip-if(!this.hasOwnProperty('Intl'))

var testDataMaximal = {
    // Used to keep "und" primary language before CLDR 44.
    "und-AQ": "en-Latn-AQ",

    // Modifies primary language.
    "und-Cyrl-RO": "bg-Cyrl-RO",
}

var testDataMinimal = {
    // Undefined primary language.
    "und": "en",
    "und-Thai": "th",
    "und-419": "es-419",
    "und-150": "en-150",
    "und-AT": "de-AT",

    // https://ssl.icu-project.org/trac/ticket/13786
    "aae-Latn-IT": "aae",
    "aae-Thai-CO": "aae-Thai-CO",

    // https://ssl.icu-project.org/trac/ticket/10220
    // https://ssl.icu-project.org/trac/ticket/12345
    "und-CW": "pap",
    "und-US": "en",
    "zh-Hant": "zh-TW",
    "zh-Hani": "zh-Hani",
};

// Add variants, extensions, and privateuse subtags and ensure they don't
// modify the result of the likely subtags algorithms.
var extras = [
    "fonipa",
    "a-not-assigned",
    "u-attr",
    "u-co",
    "u-co-phonebk",
    "x-private",
];

for (var [tag, maximal] of Object.entries(testDataMaximal)) {
    assertEq(new Intl.Locale(tag).maximize().toString(), maximal);
    assertEq(new Intl.Locale(maximal).maximize().toString(), maximal);

    for (var extra of extras) {
        assertEq(new Intl.Locale(tag + "-" + extra).maximize().toString(), maximal + "-" + extra);
    }
}

for (var [tag, minimal] of Object.entries(testDataMinimal)) {
    assertEq(new Intl.Locale(tag).minimize().toString(), minimal);
    assertEq(new Intl.Locale(minimal).minimize().toString(), minimal);

    for (var extra of extras) {
        assertEq(new Intl.Locale(tag + "-" + extra).minimize().toString(), minimal + "-" + extra);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
