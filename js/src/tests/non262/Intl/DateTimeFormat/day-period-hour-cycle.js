// |reftest| skip-if(!this.hasOwnProperty("Intl")||release_or_beta)

const {
    Year, Month, Day, DayPeriod, Hour, Minute, Literal
} = DateTimeFormatParts;

// If the locale defaults to a 24-hour-cycle, the "dayPeriod" option is ignored if an "hour" option
// is also present, unless the hour-cycle is manually set to a 12-hour-cycle.

const tests = [
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", },
        locales: {
            en: [Hour("12"), Literal(" "), DayPeriod("noon")],
            de: [Hour("12"), Literal(" Uhr")],
        },
    },
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", hour12: true },
        locales: {
            en: [Hour("12"), Literal(" "), DayPeriod("noon")],
            de: [Hour("12"), Literal(" "), DayPeriod("mittags")],
        },
    },
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", hour12: false },
        locales: {
            en: [Hour("12")],
            de: [Hour("12"), Literal(" Uhr")],
        },
    },
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", hourCycle: "h12" },
        locales: {
            en: [Hour("12"), Literal(" "), DayPeriod("noon")],
            de: [Hour("12"), Literal(" "), DayPeriod("mittags")],
        },
    },
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", hourCycle: "h11" },
        locales: {
            en: [Hour("0"), Literal(" "), DayPeriod("noon")],
            de: [Hour("0"), Literal(" "), DayPeriod("mittags")],
        },
    },
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", hourCycle: "h23" },
        locales: {
            en: [Hour("12")],
            de: [Hour("12"), Literal(" Uhr")],
        },
    },
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", hour: "numeric", hourCycle: "h24" },
        locales: {
            en: [Hour("12")],
            de: [Hour("12"), Literal(" Uhr")],
        },
    },

    // The default hour-cycle is irrelevant when an "hour" option isn't present.
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", day: "numeric", month: "numeric", year: "numeric" },
        locales: {
            en: [Month("1"), Literal("/"), Day("1"), Literal("/"), Year("2019"), Literal(", "), DayPeriod("noon")],
            de: [Day("1"), Literal("."), Month("1"), Literal("."), Year("2019"), Literal(", "), DayPeriod("mittags")],
        },
    },

    // ICU replacement pattern for missing <appendItem> entries in CLDR.
    {
        date: new Date("2019-01-01T12:00:00"),
        options: { dayPeriod: "short", minute: "numeric" },
        locales: {
            en: [Minute("0"), Literal(" ├AM/PM: "), DayPeriod("noon"), Literal("┤")],
            de: [Minute("0"), Literal(" ├Tageshälfte: "), DayPeriod("mittags"), Literal("┤")],
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
