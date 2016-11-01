// |reftest| skip-if(!this.hasOwnProperty("Intl"))

const defaultLocale = "en-US";
const defaultDate = Date.UTC(2012, 12-1, 6, 12, 0, 0);
const defaultOptions = {};

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
        timeZone: "Europe/Berlin", locale: "de",
        options: { timeZoneName: "short" },
        result: "6.12.2012, 13:00:00 MEZ",
    },
    {
        timeZone: "Europe/Paris", locale: "fr",
        options: { timeZoneName: "long" },
        result: "06/12/2012 à 13:00:00 heure normale d’Europe centrale",
    },
    {
        timeZone: "Asia/Shanghai", locale: "zh-Hans-CN",
        options: { timeZoneName: "long" },
        result: "2012/12/6 中国标准时间 下午8:00:00",
    },
    {
        timeZone: { toString: () => "Australia/Melbourne" }, locale: "en-AU",
        result: "06/12/2012, 11:00:00 pm",
    },
];

for (let {timeZone, result, locale = defaultLocale, date = defaultDate, options = defaultOptions} of tests) {
    let s = new Date(date).toLocaleString(locale, Object.assign({timeZone}, options));
    assertEq(s, result);
}


// |undefined| or absent "timeZone" option selects the default time zone.
{
    let locale = defaultLocale;
    let date = defaultDate;
    let options = defaultOptions;

    let absentTz = new Date(date).toLocaleString(locale, Object.assign({}, options));
    let undefinedTz = new Date(date).toLocaleString(locale, Object.assign({timeZone: undefined}, options));
    assertEq(undefinedTz, absentTz);
}


// RangeError is thrown for invalid time zone names.
for (let timeZone of ["", "undefined", "UTC\0", "Vienna", "Africa", "America/NewYork"]) {
    assertThrowsInstanceOf(() => {
        new Date(defaultDate).toLocaleString(undefined, {timeZone});
    }, RangeError);
}

// RangeError is thrown for these values, because ToString(<value>)
// isn't a valid time zone name.
for (let timeZone of [null, 0, 0.5, true, false, [], {}, function() {}]) {
    assertThrowsInstanceOf(() => {
        new Date(defaultDate).toLocaleString(undefined, {timeZone});
    }, RangeError);
}

// ToString(<symbol>) throws TypeError.
assertThrowsInstanceOf(() => {
    new Date(defaultDate).toLocaleString(undefined, {timeZone: Symbol()});
}, TypeError);


if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
