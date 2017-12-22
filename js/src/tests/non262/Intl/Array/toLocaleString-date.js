if (typeof Intl === "object") {
    const localeSep = [,,].toLocaleString();

    const date = new Date(Date.UTC(2012, 11, 12, 3, 0, 0));

    assertEq([date].toLocaleString("en-us", {timeZone: "UTC"}), "12/12/2012, 3:00:00 AM");
    assertEq([date].toLocaleString(["de", "en"], {timeZone: "UTC"}), "12.12.2012, 03:00:00");
    assertEq([date].toLocaleString("th-th", {timeZone: "UTC"}), "12/12/2555 03:00:00");
    assertEq([date].toLocaleString("th-th-u-nu-thai", {timeZone: "UTC"}), "๑๒/๑๒/๒๕๕๕ ๐๓:๐๐:๐๐");

    const sampleValues = [
        date, new Date(0),
    ];
    const sampleLocales = [
        void 0,
        "en",
        "th-th-u-nu-thai",
        "ja-jp",
        "ar-ma-u-ca-islamicc",
        ["tlh", "de"],
    ];
    const numericFormatOptions = {
        timeZone: "UTC",
        year: "numeric", month: "numeric", day: "numeric",
        hour: "numeric", minute: "numeric", second: "numeric",
    };
    const longFormatOptions = {
        timeZone: "UTC",
        year: "numeric", month: "long", day: "numeric",
        hour: "numeric", minute: "numeric", second: "numeric"
    };
    const sampleOptions = [
        {timeZone: "UTC"},
        longFormatOptions,
    ];

    for (let locale of sampleLocales) {
        for (let options of sampleOptions) {
            let dtfOptions;
            if (options === longFormatOptions) {
                dtfOptions = longFormatOptions;
            } else {
                dtfOptions = numericFormatOptions;
            }
            let dtf = new Intl.DateTimeFormat(locale, dtfOptions);
            let expected = sampleValues.map(dtf.format).join(localeSep);
            assertEq(sampleValues.toLocaleString(locale, options), expected);
        }
    }
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
