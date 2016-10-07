// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const defaultLocale = "en-US";
const defaultDate = Date.UTC(2012, 12-1, 6, 12, 0, 0);
const defaultOptions = {
    year: "numeric", month: "numeric", day: "numeric",
    hour: "numeric", minute: "numeric", second: "numeric",
};
const longFormatOptions = Object.assign({}, defaultOptions, {
    month: "long"
});
const tzNameFormatOptions = Object.assign({}, defaultOptions, {
    timeZoneName: "short"
});

const tzMapper = [
    x => x,
    x => x.toUpperCase(),
    x => x.toLowerCase(),
];

const tests = [
    {
        timeZone: "UTC",
        result: "12/6/2012, 12:00:00 PM",
    },
    {
        timeZone: "America/Los_Angeles",
        result: "12/6/2012, 4:00:00 AM",
    },
    {
        timeZone: "America/New_York",
        options: tzNameFormatOptions,
        result: "12/6/2012, 7:00:00 AM EST",
    },
    {
        timeZone: "America/Caracas",
        result: "12/6/2012, 7:30:00 AM",
    },
    {
        timeZone: "Europe/London",
        result: "12/6/2012, 12:00:00 PM",
    },
    {
        timeZone: "Africa/Casablanca",
        locale: "ar-MA-u-ca-islamicc", options: longFormatOptions,
        result: "22 محرم، 1434 12:00:00",
    },
    {
        timeZone: "Europe/Berlin",
        locale: "de-DE", options: tzNameFormatOptions,
        result: "6.12.2012, 13:00:00 MEZ",
    },
    {
        timeZone: "Asia/Kathmandu",
        result: "12/6/2012, 5:45:00 PM",
    },
    {
        timeZone: "Asia/Bangkok",
        locale: "th-th-u-nu-thai", options: longFormatOptions,
        result: "๖ ธันวาคม ๒๕๕๕ ๑๙:๐๐:๐๐",
    },
    {
        timeZone: "Asia/Tokyo",
        locale: "ja-JP", options: longFormatOptions,
        result: "2012年12月6日 21:00:00",
    },
    {
        timeZone: "Australia/Lord_Howe",
        result: "12/6/2012, 11:00:00 PM",
    },
    {
        timeZone: "Australia/Lord_Howe",
        date: Date.UTC(2012, 7-1, 6, 12, 0, 0),
        result: "7/6/2012, 10:30:00 PM",
    },
    {
        timeZone: "Pacific/Kiritimati",
        date: Date.UTC(1978, 12-1, 6, 12, 0, 0),
        result: "12/6/1978, 1:20:00 AM",
    },
    {
        timeZone: "Africa/Monrovia",
        date: Date.UTC(1971, 12-1, 6, 12, 0, 0),
        result: "12/6/1971, 11:15:30 AM",
    },
    {
        timeZone: "Asia/Riyadh",
        date: Date.UTC(1946, 12-1, 6, 12, 0, 0),
        result: "12/6/1946, 3:06:52 PM",
    },
];

for (let {timeZone, result, locale = defaultLocale, date = defaultDate, options = defaultOptions} of tests) {
    for (let map of tzMapper) {
        let dtf = new Intl.DateTimeFormat(locale, Object.assign({timeZone: map(timeZone)}, options));
        assertEq(dtf.format(date), result);
        assertEq(dtf.resolvedOptions().timeZone, timeZone);
    }
}


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
