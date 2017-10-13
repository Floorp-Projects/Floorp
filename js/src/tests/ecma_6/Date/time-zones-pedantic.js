// |reftest| skip-if(xulRuntime.OS=="WINNT"||xulRuntime.OS=="Darwin") -- Skip on OS X in addition to Windows

// Contains the tests from "time-zones.js" which fail on OS X. 

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
        setTimeZone(undefined);
    }
}

const weekdays = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"].join("|");
const months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"].join("|");
const datePart = String.raw `(?:${weekdays}) (?:${months}) \d{2}`;
const timePart = String.raw `\d{4,6} \d{2}:\d{2}:\d{2} GMT[+-]\d{4}`;
const dateTimeRE = new RegExp(String.raw `^(${datePart} ${timePart})(?: \((.+)\))?$`);

function assertDateTime(date, expected, alternativeTimeZone = undefined) {
    let actual = date.toString();
    assertEq(dateTimeRE.test(expected), true, `${expected}`);
    assertEq(dateTimeRE.test(actual), true, `${actual}`);

    let [, expectedDateTime, expectedTimeZone] = dateTimeRE.exec(expected);
    let [, actualDateTime, actualTimeZone] = dateTimeRE.exec(actual);

    assertEq(actualDateTime, expectedDateTime);

    // The time zone identifier is optional, so only compare its value if it's
    // present in |actual| and |expected|.
    if (expectedTimeZone !== undefined && actualTimeZone !== undefined) {
        // Test against the alternative time zone identifier if necessary.
        if (actualTimeZone !== expectedTimeZone && alternativeTimeZone !== undefined) {
            expectedTimeZone = alternativeTimeZone;
        }
        assertEq(actualTimeZone, expectedTimeZone);
    }
}

// bug 637244
inTimeZone("Asia/Novosibirsk", () => {
    let dt1 = new Date(1984, Month.April, 1, -1);
    assertDateTime(dt1, "Sat Mar 31 1984 23:00:00 GMT+0700 (NOVT)", "+07");

    let dt2 = new Date(1984, Month.April, 1);
    assertDateTime(dt2, "Sun Apr 01 1984 01:00:00 GMT+0800 (NOVST)", "+08");
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
