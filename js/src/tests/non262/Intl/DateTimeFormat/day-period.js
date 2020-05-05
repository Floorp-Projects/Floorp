// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const {
    Weekday, DayPeriod, Literal
} = DateTimeFormatParts;

const tests = [
    // https://unicode-org.atlassian.net/browse/ICU-20741
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "long", weekday: "long", },
        locales: {
            "en-001": [Weekday("Tuesday"), Literal(", "), DayPeriod("noon")],
        },
    },

    // https://unicode-org.atlassian.net/browse/ICU-20740
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "narrow", weekday: "long", },
        locales: {
            "bs-Cyrl": [Weekday("уторак"), Literal(" "), DayPeriod("подне")],
        },
    },
];

for (let {date, options, locales} of tests) {
    for (let [locale, parts] of Object.entries(locales)) {
        let dtf = new Intl.DateTimeFormat(locale, options);

        assertEq(dtf.format(date), parts.map(({value}) => value).join(""),
                 `locale=${locale}, date=${date}, options=${JSON.stringify(options)}`);

        assertDeepEq(dtf.formatToParts(date), parts,
                     `locale=${locale}, date=${date}, options=${JSON.stringify(options)}`);
    }
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
