// |reftest| skip-if(!this.hasOwnProperty('Intl')||(!this.Intl.Locale&&!this.hasOwnProperty('addIntlExtras')))

var testData = [
    {
        tag: "cel-gaulish",
        options: {
            numberingSystem: "latn",
        },
        canonical: "xtg-u-nu-latn-x-cel-gaulish",
        extensions: {
            numberingSystem: "latn",
        },
    },

    {
        tag: "cel-gaulish",
        options: {
            region: "FR",
            numberingSystem: "latn",
        },
        canonical: "xtg-FR-u-nu-latn-x-cel-gaulish",
        extensions: {
            numberingSystem: "latn",
        },
    },

    {
        tag: "art-lojban",
        options: {
            numberingSystem: "latn",
        },
        canonical: "jbo-u-nu-latn",
        extensions: {
            numberingSystem: "latn",
        },
    },

    {
        tag: "art-lojban",
        options: {
            region: "ZZ",
            numberingSystem: "latn",
        },
        canonical: "jbo-ZZ-u-nu-latn",
        extensions: {
            numberingSystem: "latn",
        },
    },
];

for (var {tag, options, canonical, extensions} of testData) {
    var loc = new Intl.Locale(tag, options);
    assertEq(loc.toString(), canonical);

    for (var [name, value] of Object.entries(extensions)) {
        assertEq(loc[name], value);
    }
}

var errorTestData = [
    "en-gb-oed",
    "i-default",
    "sgn-ch-de",
    "zh-min",
    "zh-min-nan",
    "zh-hakka-hakka",
];

for (var tag of errorTestData) {
    assertThrowsInstanceOf(() => new Intl.Locale(tag), RangeError);
    assertThrowsInstanceOf(() => new Intl.Locale(tag, {}), RangeError);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0);
