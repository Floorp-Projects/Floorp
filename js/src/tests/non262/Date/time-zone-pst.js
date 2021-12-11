// |reftest| skip-if(!xulRuntime.shell)

assertEq(/^(PST|PDT)$/.test(getTimeZone()), true,
         "The default time zone is set to PST8PDT for all jstests (when run in the shell)");

const msPerMinute = 60 * 1000;

function shortTimeZone(str) {
    return str.replace("(Pacific Standard Time)", "(PST)")
              .replace("(Pacific Daylight Time)", "(PDT)");
}

function assertEqDate(dt, date, time) {
    assertEq(shortTimeZone(dt.toString()), `${date} ${time}`);
    assertEq(dt.toDateString(), date);
    assertEq(shortTimeZone(dt.toTimeString()), time);
}

// PDT -> PST, using milliseconds from epoch.
{
    let midnight = new Date(2016, Month.November, 6, 0, 0, 0, 0);
    let midnightUTC = Date.UTC(2016, Month.November, 6, 0, 0, 0, 0);

    // Ensure midnight time is correct.
    assertEq(midnightUTC - midnight.getTime(), -7 * msPerHour);

    let tests = [
        { offset: 0 * 60,      date: "Sun Nov 06 2016", time: "00:00:00 GMT-0700 (PDT)" },
        { offset: 0 * 60 + 30, date: "Sun Nov 06 2016", time: "00:30:00 GMT-0700 (PDT)" },
        { offset: 1 * 60,      date: "Sun Nov 06 2016", time: "01:00:00 GMT-0700 (PDT)" },
        { offset: 1 * 60 + 30, date: "Sun Nov 06 2016", time: "01:30:00 GMT-0700 (PDT)" },
        { offset: 2 * 60,      date: "Sun Nov 06 2016", time: "01:00:00 GMT-0800 (PST)" },
        { offset: 2 * 60 + 30, date: "Sun Nov 06 2016", time: "01:30:00 GMT-0800 (PST)" },
        { offset: 3 * 60,      date: "Sun Nov 06 2016", time: "02:00:00 GMT-0800 (PST)" },
        { offset: 3 * 60 + 30, date: "Sun Nov 06 2016", time: "02:30:00 GMT-0800 (PST)" },
        { offset: 4 * 60,      date: "Sun Nov 06 2016", time: "03:00:00 GMT-0800 (PST)" },
        { offset: 4 * 60 + 30, date: "Sun Nov 06 2016", time: "03:30:00 GMT-0800 (PST)" },
    ];

    for (let {offset, date, time} of tests) {
        let dt = new Date(midnight.getTime() + offset * msPerMinute);
        assertEqDate(dt, date, time);
    }
}


// PDT -> PST, using local date-time.
{
    let tests = [
        { offset: 0 * 60,      date: "Sun Nov 06 2016", time: "00:00:00 GMT-0700 (PDT)" },
        { offset: 0 * 60 + 30, date: "Sun Nov 06 2016", time: "00:30:00 GMT-0700 (PDT)" },
        { offset: 1 * 60,      date: "Sun Nov 06 2016", time: "01:00:00 GMT-0700 (PDT)" },
        { offset: 1 * 60 + 30, date: "Sun Nov 06 2016", time: "01:30:00 GMT-0700 (PDT)" },
        { offset: 2 * 60,      date: "Sun Nov 06 2016", time: "02:00:00 GMT-0800 (PST)" },
        { offset: 2 * 60 + 30, date: "Sun Nov 06 2016", time: "02:30:00 GMT-0800 (PST)" },
        { offset: 3 * 60,      date: "Sun Nov 06 2016", time: "03:00:00 GMT-0800 (PST)" },
        { offset: 3 * 60 + 30, date: "Sun Nov 06 2016", time: "03:30:00 GMT-0800 (PST)" },
        { offset: 4 * 60,      date: "Sun Nov 06 2016", time: "04:00:00 GMT-0800 (PST)" },
        { offset: 4 * 60 + 30, date: "Sun Nov 06 2016", time: "04:30:00 GMT-0800 (PST)" },
    ];

    for (let {offset, date, time} of tests) {
        let dt = new Date(2016, Month.November, 6, (offset / 60)|0, (offset % 60), 0, 0);
        assertEqDate(dt, date, time);
    }
}


// PST -> PDT, using milliseconds from epoch.
{
    let midnight = new Date(2016, Month.March, 13, 0, 0, 0, 0);
    let midnightUTC = Date.UTC(2016, Month.March, 13, 0, 0, 0, 0);

    // Ensure midnight time is correct.
    assertEq(midnightUTC - midnight.getTime(), -8 * msPerHour);

    let tests = [
        { offset: 0 * 60,      date: "Sun Mar 13 2016", time: "00:00:00 GMT-0800 (PST)" },
        { offset: 0 * 60 + 30, date: "Sun Mar 13 2016", time: "00:30:00 GMT-0800 (PST)" },
        { offset: 1 * 60,      date: "Sun Mar 13 2016", time: "01:00:00 GMT-0800 (PST)" },
        { offset: 1 * 60 + 30, date: "Sun Mar 13 2016", time: "01:30:00 GMT-0800 (PST)" },
        { offset: 2 * 60,      date: "Sun Mar 13 2016", time: "03:00:00 GMT-0700 (PDT)" },
        { offset: 2 * 60 + 30, date: "Sun Mar 13 2016", time: "03:30:00 GMT-0700 (PDT)" },
        { offset: 3 * 60,      date: "Sun Mar 13 2016", time: "04:00:00 GMT-0700 (PDT)" },
        { offset: 3 * 60 + 30, date: "Sun Mar 13 2016", time: "04:30:00 GMT-0700 (PDT)" },
        { offset: 4 * 60,      date: "Sun Mar 13 2016", time: "05:00:00 GMT-0700 (PDT)" },
        { offset: 4 * 60 + 30, date: "Sun Mar 13 2016", time: "05:30:00 GMT-0700 (PDT)" },
    ];

    for (let {offset, date, time} of tests) {
        let dt = new Date(midnight.getTime() + offset * msPerMinute);
        assertEqDate(dt, date, time);
    }
}


// PST -> PDT, using local date-time.
{
    let tests = [
        { offset: 0 * 60,      date: "Sun Mar 13 2016", time: "00:00:00 GMT-0800 (PST)" },
        { offset: 0 * 60 + 30, date: "Sun Mar 13 2016", time: "00:30:00 GMT-0800 (PST)" },
        { offset: 1 * 60,      date: "Sun Mar 13 2016", time: "01:00:00 GMT-0800 (PST)" },
        { offset: 1 * 60 + 30, date: "Sun Mar 13 2016", time: "01:30:00 GMT-0800 (PST)" },
        { offset: 2 * 60,      date: "Sun Mar 13 2016", time: "03:00:00 GMT-0700 (PDT)" },
        { offset: 2 * 60 + 30, date: "Sun Mar 13 2016", time: "03:30:00 GMT-0700 (PDT)" },
        { offset: 3 * 60,      date: "Sun Mar 13 2016", time: "03:00:00 GMT-0700 (PDT)" },
        { offset: 3 * 60 + 30, date: "Sun Mar 13 2016", time: "03:30:00 GMT-0700 (PDT)" },
        { offset: 4 * 60,      date: "Sun Mar 13 2016", time: "04:00:00 GMT-0700 (PDT)" },
        { offset: 4 * 60 + 30, date: "Sun Mar 13 2016", time: "04:30:00 GMT-0700 (PDT)" },
    ];

    for (let {offset, date, time} of tests) {
        let dt = new Date(2016, Month.March, 13, (offset / 60)|0, (offset % 60), 0, 0);
        assertEqDate(dt, date, time);
    }
}


if (typeof reportCompare === "function")
    reportCompare(true, true);
