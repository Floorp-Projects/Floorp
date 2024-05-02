let dates = [
    "0217-09-23", "+000217-09-23", "-000217-09-23",
    "2017-09-23", "+002017-09-23", "-002017-09-23",
                  "+022017-09-23", "-022017-09-23",
                  "+202017-09-23", "-202017-09-23",
];

for (let date of dates) {
    let d = new Date(date);
    let timeValue = d.valueOf();

    assertEq(Number.isNaN(timeValue), false, `Cannot parse "${date}" as ISO date-only form`);

    // Ensure parsing the results of toString(), toUTCString(), and toISOString()
    // gives the same time value as required by 20.3.3.2 Date.parse.
    //
    // Additional requirement not present in ES2019 draft rev 7acacc524213058a2368b5fa751fac7ea08ba230:
    // The time zone offset must not contain seconds (or milliseconds) for |Date.parse(x.toString())|
    // to be equal to |x.valueOf()|.
    let tz = d.getTimezoneOffset();
    if (Math.trunc(tz) === tz) {
        assertEq(Date.parse(d.toString()), timeValue, `Cannot parse from toString() of "${date}"`);
    }
    assertEq(Date.parse(d.toUTCString()), timeValue, `Cannot parse from toUTCString() of "${date}"`);
    assertEq(Date.parse(d.toISOString()), timeValue, `Cannot parse from toISOString() of "${date}"`);
}

if (typeof reportCompare === "function")
    reportCompare(true, true);
