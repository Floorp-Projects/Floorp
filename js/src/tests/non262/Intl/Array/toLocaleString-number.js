if (typeof Intl === "object") {
    const localeSep = [,,].toLocaleString();

    assertEq([NaN].toLocaleString("ar"), "ليس رقم");
    assertEq([NaN].toLocaleString(["zh-hant", "ar"]), "非數值");
    assertEq([Infinity].toLocaleString("dz"), "གྲངས་མེད");
    assertEq([-Infinity].toLocaleString(["fr", "en"]), "-∞");

    const sampleValues = [
        -0, +0, -1, +1, -2, +2, -0.5, +0.5,
    ];
    const sampleLocales = [
        void 0,
        "en",
        "th-th-u-nu-thai",
        ["tlh", "de"],
    ];
    const sampleOptions = [
        void 0,
        {},
        {style: "percent"},
        {style: "currency", currency: "USD", minimumIntegerDigits: 4},
    ];
    for (let locale of sampleLocales) {
        for (let options of sampleOptions) {
            let nf = new Intl.NumberFormat(locale, options);
            let expected = sampleValues.map(nf.format).join(localeSep);
            assertEq(sampleValues.toLocaleString(locale, options), expected);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
