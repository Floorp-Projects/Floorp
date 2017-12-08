// |reftest| skip-if(xulRuntime.OS=="WINNT"&&!xulRuntime.shell) -- Windows browser in automation doesn't pick up new time zones correctly

// Repeats the test from "time-zones.js", but uses POSIX instead of IANA names
// for the time zones. This allows to run these tests on Windows, too.

// From bug 1330149:
//
// Windows only supports a very limited set of IANA time zone names for the TZ
// environment variable.
//
// TZ format supported by Windows: "TZ=tzn[+|-]hh[:mm[:ss]][dzn]".
//
// Complete list of all IANA time zone ids matching that format.
//
// From tzdata's "northamerica" file:
//   EST5EDT
//   CST6CDT
//   MST7MDT
//   PST8PDT
//
// From tzdata's "backward" file:
//   GMT+0
//   GMT-0
//   GMT0
//
// Also supported on Windows even though they don't match the format listed
// above.
//
// From tzdata's "backward" file:
//   UCT
//   UTC
//
// From tzdata's "etcetera" file:
//   GMT


// Perform the following replacements:
//   America/New_York    -> EST5EDT
//   America/Chicago     -> CST6CDT
//   America/Denver      -> MST7MDT
//   America/Los_Angeles -> PST8PDT
//
// And remove any tests not matching one of the four time zones from above.

const msPerHour = 60 * 60 * 1000;

const Month = {
    January: 0,
    February: 1,
    March: 2,
    April: 3,
    May: 4,
    June: 5,
    July: 6,
    August: 7,
    September: 8,
    October: 9,
    November: 10,
    December: 11,
};

function inTimeZone(tzname, fn) {
    setTimeZone(tzname);
    try {
        fn();
    } finally {
        setTimeZone("PST8PDT");
    }
}

const weekdays = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"].join("|");
const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"].join("|");
const datePart = String.raw `(?:${weekdays}) (?:${months}) \d{2}`;
const timePart = String.raw `\d{4,6} \d{2}:\d{2}:\d{2} GMT[+-]\d{4}`;
const dateTimeRE = new RegExp(String.raw `^(${datePart} ${timePart})(?: \((.+)\))?$`);

function assertDateTime(date, expected) {
    let actual = date.toString();
    assertEq(dateTimeRE.test(expected), true, `${expected}`);
    assertEq(dateTimeRE.test(actual), true, `${actual}`);

    let [, expectedDateTime, expectedTimeZone] = dateTimeRE.exec(expected);
    let [, actualDateTime, actualTimeZone] = dateTimeRE.exec(actual);

    assertEq(actualDateTime, expectedDateTime);

    // The time zone identifier is optional, so only compare its value if it's
    // present in |actual| and |expected|.
    if (expectedTimeZone !== undefined && actualTimeZone !== undefined) {
        assertEq(actualTimeZone, expectedTimeZone);
    }
}

// bug 294908
inTimeZone("EST5EDT", () => {
    let dt = new Date(2003, Month.April, 6, 2, 30, 00);
    assertDateTime(dt, "Sun Apr 06 2003 03:30:00 GMT-0400 (EDT)");
});

// bug 610183
inTimeZone("PST8PDT", () => {
    let dt = new Date(2014, Month.November, 2, 1, 47, 42);
    assertDateTime(dt, "Sun Nov 02 2014 01:47:42 GMT-0700 (PDT)");
});

// bug 629465
inTimeZone("MST7MDT", () => {
    let dt1 = new Date(Date.UTC(2015, Month.November, 1, 0, 0, 0) + 6 * msPerHour);
    assertDateTime(dt1, "Sun Nov 01 2015 00:00:00 GMT-0600 (MDT)");

    let dt2 = new Date(Date.UTC(2015, Month.November, 1, 1, 0, 0) + 6 * msPerHour);
    assertDateTime(dt2, "Sun Nov 01 2015 01:00:00 GMT-0600 (MDT)");

    let dt3 = new Date(Date.UTC(2015, Month.November, 1, 1, 0, 0) + 7 * msPerHour);
    assertDateTime(dt3, "Sun Nov 01 2015 01:00:00 GMT-0700 (MST)");
});

// bug 742427
inTimeZone("EST5EDT", () => {
    let dt = new Date(2009, Month.March, 8, 1, 0, 0);
    assertDateTime(dt, "Sun Mar 08 2009 01:00:00 GMT-0500 (EST)");
    dt.setHours(dt.getHours() + 1);
    assertDateTime(dt, "Sun Mar 08 2009 03:00:00 GMT-0400 (EDT)");
});
inTimeZone("MST7MDT", () => {
    let dt = new Date(2009, Month.March, 8, 1, 0, 0);
    assertDateTime(dt, "Sun Mar 08 2009 01:00:00 GMT-0700 (MST)");
    dt.setHours(dt.getHours() + 1);
    assertDateTime(dt, "Sun Mar 08 2009 03:00:00 GMT-0600 (MDT)");
});
inTimeZone("EST5EDT", () => {
    let dt1 = new Date(Date.UTC(2008, Month.March, 9, 0, 0, 0) + 5 * msPerHour);
    assertDateTime(dt1, "Sun Mar 09 2008 00:00:00 GMT-0500 (EST)");

    let dt2 = new Date(Date.UTC(2008, Month.March, 9, 1, 0, 0) + 5 * msPerHour);
    assertDateTime(dt2, "Sun Mar 09 2008 01:00:00 GMT-0500 (EST)");

    let dt3 = new Date(Date.UTC(2008, Month.March, 9, 4, 0, 0) + 4 * msPerHour);
    assertDateTime(dt3, "Sun Mar 09 2008 04:00:00 GMT-0400 (EDT)");
});

// bug 802627
inTimeZone("EST5EDT", () => {
    let dt = new Date(0);
    assertDateTime(dt, "Wed Dec 31 1969 19:00:00 GMT-0500 (EST)");
});

// bug 879261
inTimeZone("EST5EDT", () => {
    let dt1 = new Date(1362891600000);
    assertDateTime(dt1, "Sun Mar 10 2013 00:00:00 GMT-0500 (EST)");

    let dt2 = new Date(dt1.setHours(dt1.getHours() + 24));
    assertDateTime(dt2, "Mon Mar 11 2013 00:00:00 GMT-0400 (EDT)");
});
inTimeZone("PST8PDT", () => {
    let dt1 = new Date(2014, Month.January, 1);
    assertDateTime(dt1, "Wed Jan 01 2014 00:00:00 GMT-0800 (PST)");

    let dt2 = new Date(2014, Month.August, 1);
    assertDateTime(dt2, "Fri Aug 01 2014 00:00:00 GMT-0700 (PDT)");
});
inTimeZone("EST5EDT", () => {
    let dt1 = new Date(2016, Month.October, 14, 3, 5, 9);
    assertDateTime(dt1, "Fri Oct 14 2016 03:05:09 GMT-0400 (EDT)");

    let dt2 = new Date(2016, Month.January, 9, 23, 26, 40);
    assertDateTime(dt2, "Sat Jan 09 2016 23:26:40 GMT-0500 (EST)");
});

// bug 1084547
inTimeZone("EST5EDT", () => {
    let dt = new Date(Date.parse("2014-11-02T02:00:00-04:00"));
    assertDateTime(dt, "Sun Nov 02 2014 01:00:00 GMT-0500 (EST)");

    dt.setMilliseconds(0);
    assertDateTime(dt, "Sun Nov 02 2014 01:00:00 GMT-0400 (EDT)");
});

// bug 1303306
inTimeZone("EST5EDT", () => {
    let dt = new Date(2016, Month.September, 15, 16, 14, 48);
    assertDateTime(dt, "Thu Sep 15 2016 16:14:48 GMT-0400 (EDT)");
});

// bug 1317364
inTimeZone("PST8PDT", () => {
    let dt = new Date(2016, Month.March, 13, 2, 30, 0, 0);
    assertDateTime(dt, "Sun Mar 13 2016 03:30:00 GMT-0700 (PDT)");

    let dt2 = new Date(2016, Month.January, 5, 0, 30, 30, 500);
    assertDateTime(dt2, "Tue Jan 05 2016 00:30:30 GMT-0800 (PST)");

    let dt3 = new Date(dt2.getTime());
    dt3.setMonth(dt2.getMonth() + 2);
    dt3.setDate(dt2.getDate() + 7 + 1);
    dt3.setHours(dt2.getHours() + 2);

    assertEq(dt3.getHours(), 3);
});

// bug 1355272
inTimeZone("PST8PDT", () => {
    let dt = new Date(2017, Month.April, 10, 17, 25, 07);
    assertDateTime(dt, "Mon Apr 10 2017 17:25:07 GMT-0700 (PDT)");
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
