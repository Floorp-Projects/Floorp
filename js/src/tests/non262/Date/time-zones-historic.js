// |reftest| skip-if(winWidget||!this.hasOwnProperty("Intl")) -- Windows doesn't accept IANA names for the TZ env variable; Requires ICU time zone support

// bug 487897
inTimeZone("Europe/London", () => {
    let dt1 = new Date(1970, Month.January, 1, 0, 0, 0, 0);
    assertDateTime(dt1, "Thu Jan 01 1970 00:00:00 GMT+0100 (Greenwich Mean Time)");
    assertEq(dt1.getHours(), 0);

    let dt2 = new Date(1915, Month.January, 1);
    assertDateTime(dt2, "Fri Jan 01 1915 00:00:00 GMT+0000 (Greenwich Mean Time)");

    let dt3 = new Date(1970, Month.January, 1);
    assertDateTime(dt3, "Thu Jan 01 1970 00:00:00 GMT+0100 (Greenwich Mean Time)");
});

// bug 637244
inTimeZone("Asia/Novosibirsk", () => {
    let dt1 = new Date("1942-03-01T00:00:00");
    assertDateTime(dt1, "Sun Mar 01 1942 00:00:00 GMT+0700 (Novosibirsk Standard Time)");
    dt1.setMonth(Month.April);
    assertDateTime(dt1, "Wed Apr 01 1942 00:00:00 GMT+0700 (Novosibirsk Standard Time)");

    let dt2 = new Date(2010, Month.October, 31);
    assertDateTime(dt2, "Sun Oct 31 2010 00:00:00 GMT+0700 (Novosibirsk Summer Time)");
    dt2.setMonth(Month.November);
    assertDateTime(dt2, "Wed Dec 01 2010 00:00:00 GMT+0600 (Novosibirsk Standard Time)");

    let dt3 = new Date(1942, Month.April, 1);
    assertDateTime(dt3, "Wed Apr 01 1942 00:00:00 GMT+0700 (Novosibirsk Standard Time)");

    function getNumberOfDaysInMonth(year, month) {
        switch (month) {
          case Month.January:
          case Month.March:
          case Month.May:
          case Month.July:
          case Month.August:
          case Month.October:
          case Month.December:
            return 31;
          case Month.April:
          case Month.June:
          case Month.September:
          case Month.November:
            return 30;
          case Month.February:
            if (year % 4 === 0 && (year % 100 !== 0 || year % 400 === 0))
                return 29;
            return 28;
        }
        throw new Error(`Illegal month value: ${month}`);
    }

    for (let year = 1900; year <= 2142; ++year) {
        for (let month = Month.January; month <= Month.December; ++month) {
            const numDays = getNumberOfDaysInMonth(year, month);
            for (let day = 1; day <= numDays; ++day) {
                let date = new Date(year, month, day);
                assertEq(date.getMonth(), month);
            }
        }
    }

    let dt4 = new Date(1984, Month.April, 1);
    assertDateTime(dt4, "Sun Apr 01 1984 01:00:00 GMT+0800 (Novosibirsk Summer Time)");

    let dt5 = new Date(1984, Month.March, 1);
    assertDateTime(dt5, "Thu Mar 01 1984 00:00:00 GMT+0700 (Novosibirsk Standard Time)");

    let dt6 = new Date(1984, Month.April, 1);
    assertEq(dt6.toUTCString(), "Sat, 31 Mar 1984 17:00:00 GMT");
    assertEq(dt6.getTime(), 449600400000);
});
inTimeZone("Europe/Tallinn", () => {
    let dt = new Date(2040, Month.March, 31, 20);

    for (let {datetime, date, hours} of [
        {datetime: "Sat Mar 31 2040 20:00:00", date: 31, hours: 20},
        {datetime: "Sat Mar 31 2040 22:00:00", date: 31, hours: 22},
        {datetime: "Sun Apr 01 2040 00:00:00", date: 1, hours: 0},
        {datetime: "Sun Apr 01 2040 02:00:00", date: 1, hours: 2},
        {datetime: "Sun Apr 01 2040 04:00:00", date: 1, hours: 4},
        {datetime: "Sun Apr 01 2040 06:00:00", date: 1, hours: 6},
        {datetime: "Sun Apr 01 2040 08:00:00", date: 1, hours: 8},
    ]) {
        assertDateTime(dt, `${datetime} GMT+0300 (Eastern European Summer Time)`);
        assertEq(dt.getDate(), date);
        assertEq(dt.getHours(), hours);
        assertEq(dt.getTimezoneOffset(), -180);

        dt.setHours(dt.getHours() + 2);
    }
});
inTimeZone("Europe/Riga", () => {
    let dt1 = new Date(2016, Month.March, 27, 2, 59);
    assertDateTime(dt1, "Sun Mar 27 2016 02:59:00 GMT+0200 (Eastern European Standard Time)");

    let dt2 = new Date(2016, Month.March, 27, 3, 0);
    assertDateTime(dt2, "Sun Mar 27 2016 04:00:00 GMT+0300 (Eastern European Summer Time)");
});

// bug 704486
inTimeZone("Europe/Zagreb", () => {
    let dt = new Date(Date.UTC(1942, Month.June, 11, 22, 0, 0, 0));
    assertDateTime(dt, "Fri Jun 12 1942 00:00:00 GMT+0200 (Central European Summer Time)");
    assertEq(dt.getFullYear(), 1942);
    assertEq(dt.getMonth(), Month.June);
    assertEq(dt.getDate(), 12);
    assertEq(dt.getHours(), 0);
    assertEq(dt.getMinutes(), 0);
    assertEq(dt.getSeconds(), 0);
    assertEq(dt.getMilliseconds(), 0);
    assertEq(dt.getTimezoneOffset(), -120);
});

// bug 935909
inTimeZone("Europe/London", () => {
    let dt1 = new Date(1954, Month.January, 1);
    assertDateTime(dt1, "Fri Jan 01 1954 00:00:00 GMT+0000 (Greenwich Mean Time)");

    let dt2 = new Date(1965, Month.January, 1);
    assertDateTime(dt2, "Fri Jan 01 1965 00:00:00 GMT+0000 (Greenwich Mean Time)");

    let dt3 = new Date(1970, Month.January, 1);
    assertDateTime(dt3, "Thu Jan 01 1970 00:00:00 GMT+0100 (Greenwich Mean Time)");

    let dt4 = new Date(-504921600000);
    assertDateTime(dt4, "Fri Jan 01 1954 00:00:00 GMT+0000 (Greenwich Mean Time)");

    let dt5 = new Date(1974, Month.January, 1);
    assertDateTime(dt5, "Tue Jan 01 1974 00:00:00 GMT+0000 (Greenwich Mean Time)");
});
inTimeZone("Europe/Dublin", () => {
    let dt = new Date(0, Month.January, 1, 0, 0, 0, 0);
    dt.setFullYear(1970);
    assertEq(dt.getFullYear(), 1970);
});

// bug 937261
inTimeZone("Europe/Lisbon", () => {
    let dt = new Date(0, Month.January, 1, 0, 0, 0, 0);
    assertDateTime(dt, "Mon Jan 01 1900 00:00:00 GMT-0036 (Western European Standard Time)");

    dt.setFullYear(2015);
    assertDateTime(dt, "Thu Jan 01 2015 00:00:00 GMT+0000 (Western European Standard Time)");

    dt.setMonth(Month.November);
    assertDateTime(dt, "Sun Nov 01 2015 00:00:00 GMT+0000 (Western European Standard Time)");
});
inTimeZone("Europe/London", () => {
    let dt = new Date(0, Month.January, 1, 0, 0, 0, 0);
    assertDateTime(dt, "Mon Jan 01 1900 00:00:00 GMT+0000 (Greenwich Mean Time)");

    dt.setFullYear(2015);
    assertDateTime(dt, "Thu Jan 01 2015 00:00:00 GMT+0000 (Greenwich Mean Time)");

    dt.setMonth(Month.November);
    assertDateTime(dt, "Sun Nov 01 2015 00:00:00 GMT+0000 (Greenwich Mean Time)");
});

// bug 1079720
inTimeZone("Europe/Moscow", () => {
    let dt1 = new Date(2014, Month.January, 1);
    assertDateTime(dt1, "Wed Jan 01 2014 00:00:00 GMT+0400 (Moscow Standard Time)");
    assertEq(dt1.toISOString(), "2013-12-31T20:00:00.000Z");
    assertEq(dt1.getHours(), 0);

    let dt2 = new Date(2013, Month.January, 1);
    assertDateTime(dt2, "Tue Jan 01 2013 00:00:00 GMT+0400 (Moscow Standard Time)");

    let dt3 = new Date(new Date(2014, Month.December, 1).setMonth(Month.January));
    assertDateTime(dt3, "Wed Jan 01 2014 00:00:00 GMT+0400 (Moscow Standard Time)");
    assertEq(dt3.getFullYear(), 2014);

    let dt4 = new Date(2040, Month.April, 1);
    assertDateTime(dt4, "Sun Apr 01 2040 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt5 = new Date(2043, Month.April, 1);
    assertDateTime(dt5, "Wed Apr 01 2043 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt6 = new Date(2054, Month.April, 1);
    assertDateTime(dt6, "Wed Apr 01 2054 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt7 = new Date(2065, Month.April, 1);
    assertDateTime(dt7, "Wed Apr 01 2065 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt8 = new Date(2068, Month.April, 1);
    assertDateTime(dt8, "Sun Apr 01 2068 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt9 = new Date(2071, Month.April, 1);
    assertDateTime(dt9, "Wed Apr 01 2071 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt10 = new Date(2082, Month.April, 1);
    assertDateTime(dt10, "Wed Apr 01 2082 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt11 = new Date(2093, Month.April, 1);
    assertDateTime(dt11, "Wed Apr 01 2093 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt12 = new Date(2096, Month.April, 1);
    assertDateTime(dt12, "Sun Apr 01 2096 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt13 = new Date(2099, Month.April, 1);
    assertDateTime(dt13, "Wed Apr 01 2099 00:00:00 GMT+0300 (Moscow Standard Time)");
});

// bug 1107837
inTimeZone("Europe/Moscow", () => {
    let dt1 = new Date(2015, Month.January, 4);
    assertDateTime(dt1, "Sun Jan 04 2015 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt2 = new Date(2015, Month.January, 5);
    assertDateTime(dt2, "Mon Jan 05 2015 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt3 = new Date(2015, Month.January, 6);
    assertDateTime(dt3, "Tue Jan 06 2015 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt4 = new Date(2015, Month.January, 7);
    assertDateTime(dt4, "Wed Jan 07 2015 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt5 = new Date(2015, Month.January, 8);
    assertDateTime(dt5, "Thu Jan 08 2015 00:00:00 GMT+0300 (Moscow Standard Time)");

    let dt6 = new Date(2015, Month.January, 9);
    assertDateTime(dt6, "Fri Jan 09 2015 00:00:00 GMT+0300 (Moscow Standard Time)");
});

// bug 1122571
inTimeZone("Europe/Berlin", () => {
    const locale = "en-001";
    const opts = {timeZoneName: "long", hour12: false};

    let dt1 = new Date(1950, Month.March, 28);
    assertDateTime(dt1, "Tue Mar 28 1950 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt1.toLocaleString(locale, opts), "28/03/1950, 00:00:00 GMT+01:00");

    let dt2 = new Date(1950, Month.July, 1);
    assertDateTime(dt2, "Sat Jul 01 1950 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt2.toLocaleString(locale, opts), "01/07/1950, 00:00:00 GMT+01:00");

    let dt3 = new Date(1960, Month.March, 27);
    assertDateTime(dt3, "Sun Mar 27 1960 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt3.toLocaleString(locale, opts), "27/03/1960, 00:00:00 GMT+01:00");

    let dt4 = new Date(1960, Month.March, 28);
    assertDateTime(dt4, "Mon Mar 28 1960 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt4.toLocaleString(locale, opts), "28/03/1960, 00:00:00 GMT+01:00");

    let dt5 = new Date(1960, Month.March, 29);
    assertDateTime(dt5, "Tue Mar 29 1960 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt5.toLocaleString(locale, opts), "29/03/1960, 00:00:00 GMT+01:00");

    let dt6 = new Date(1960, Month.July, 1);
    assertDateTime(dt6, "Fri Jul 01 1960 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt6.toLocaleString(locale, opts), "01/07/1960, 00:00:00 GMT+01:00");

    let dt7 = new Date(1961, Month.July, 1);
    assertDateTime(dt7, "Sat Jul 01 1961 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt7.toLocaleString(locale, opts), "01/07/1961, 00:00:00 GMT+01:00");

    let dt8 = new Date(1970, Month.March, 1);
    assertDateTime(dt8, "Sun Mar 01 1970 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt8.toLocaleString(locale, opts), "01/03/1970, 00:00:00 Central European Standard Time");

    let dt9 = new Date(1970, Month.March, 27);
    assertDateTime(dt9, "Fri Mar 27 1970 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt9.toLocaleString(locale, opts), "27/03/1970, 00:00:00 Central European Standard Time");

    let dt10 = new Date(1970, Month.March, 28);
    assertDateTime(dt10, "Sat Mar 28 1970 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt10.toLocaleString(locale, opts), "28/03/1970, 00:00:00 Central European Standard Time");

    let dt11 = new Date(1970, Month.July, 1);
    assertDateTime(dt11, "Wed Jul 01 1970 00:00:00 GMT+0100 (Central European Standard Time)");
    assertEq(dt11.toLocaleString(locale, opts), "01/07/1970, 00:00:00 Central European Standard Time");
});

// bug 1143398
inTimeZone("Australia/Adelaide", () => {
    let dt = new Date(621000);
    assertDateTime(dt, "Thu Jan 01 1970 09:40:21 GMT+0930 (Australian Central Standard Time)");
    assertEq(dt.getUTCFullYear(), 1970);

    dt.setMilliseconds(dt.getMilliseconds());
    dt.setSeconds(dt.getSeconds());

    assertDateTime(dt, "Thu Jan 01 1970 09:40:21 GMT+0930 (Australian Central Standard Time)");
    assertEq(dt.getUTCFullYear(), 1970);
});
inTimeZone("America/Denver", () => {
    let dt = new Date(1446361200000);
    assertDateTime(dt, "Sun Nov 01 2015 01:00:00 GMT-0600 (Mountain Daylight Time)");

    assertEq(dt.getTime(), 1446361200000);
    dt.setMilliseconds(0);
    assertEq(dt.getTime(), 1446361200000);
});

// bug 1233809
inTimeZone("America/New_York", () => {
    let dt = new Date(1980, Month.March, 10);
    assertDateTime(dt, "Mon Mar 10 1980 00:00:00 GMT-0500 (Eastern Standard Time)");
});

// bug 1254041
inTimeZone("Asia/Ho_Chi_Minh", () => {
    let dt1 = new Date(Date.UTC(1969, Month.December, 31, 23, 0));
    assertDateTime(dt1, "Thu Jan 01 1970 07:00:00 GMT+0800 (Indochina Time)");
    assertEq(dt1.getTime(), -3600000);

    dt1.setMinutes(dt1.getMinutes() + 30);
    assertDateTime(dt1, "Thu Jan 01 1970 07:30:00 GMT+0800 (Indochina Time)");
    assertEq(dt1.getTime(), -1800000);

    dt1.setMinutes(dt1.getMinutes() + 30);
    assertDateTime(dt1, "Thu Jan 01 1970 08:00:00 GMT+0800 (Indochina Time)");
    assertEq(dt1.getTime(), 0);

    dt1.setMinutes(dt1.getMinutes() + 30);
    assertDateTime(dt1, "Thu Jan 01 1970 08:30:00 GMT+0800 (Indochina Time)");
    assertEq(dt1.getTime(), 1800000);

    dt1.setMinutes(dt1.getMinutes() + 30);
    assertDateTime(dt1, "Thu Jan 01 1970 09:00:00 GMT+0800 (Indochina Time)");
    assertEq(dt1.getTime(), 3600000);

    let dt2 = new Date(-1);
    assertEq(dt2.getTime(), -1);

    dt2.setMilliseconds(dt2.getMilliseconds() + 1);
    assertEq(dt2.getTime(), 0);

    dt2.setTime(1);
    assertEq(dt2.getTime(), 1);

    dt2.setMilliseconds(dt2.getMilliseconds() - 1);
    assertEq(dt2.getTime(), 0);

    dt2.setMilliseconds(dt2.getMilliseconds() - 1);
    assertEq(dt2.getTime(), -1);

    dt2.setMilliseconds(dt2.getMilliseconds() + 1);
    assertEq(dt2.getTime(), 0);

    dt2.setMilliseconds(dt2.getMilliseconds() + 3600000);
    assertEq(dt2.getTime(), 3600000);

    dt2.setMilliseconds(dt2.getMilliseconds() + (3600000 * 2 - 1));
    assertEq(dt2.getTime(), 3600000 * 3 - 1);

    dt2.setMilliseconds(dt2.getMilliseconds() + 1);
    assertEq(dt2.getTime(), 3600000 * 3);

    dt2.setMilliseconds(dt2.getMilliseconds() + (3600000 * 2));
    assertEq(dt2.getTime(), 3600000 * 5);

    let dt3 = new Date(0);
    assertDateTime(dt3, "Thu Jan 01 1970 08:00:00 GMT+0800 (Indochina Time)");

    let dt4 = new Date(-1);
    assertDateTime(dt4, "Thu Jan 01 1970 07:59:59 GMT+0800 (Indochina Time)");
});

// bug 1300197
inTimeZone("Europe/Dublin", () => {
    let dt = new Date(1910, Month.January, 1);
    assertDateTime(dt, "Sat Jan 01 1910 00:00:00 GMT-0025 (Greenwich Mean Time)");
});

// bug 1304774
inTimeZone("Europe/Zurich", () => {
    let dt = new Date(1986, Month.April, 4, 0, 0, 0, 0);
    assertDateTime(dt, "Fri Apr 04 1986 00:00:00 GMT+0200 (Central European Summer Time)");
    assertEq(dt.getTimezoneOffset(), -120);
});

// bug 1330307
inTimeZone("Europe/Moscow", () => {
    let dt = new Date(2012, Month.May, 14, 12, 13, 14);
    assertDateTime(dt, "Mon May 14 2012 12:13:14 GMT+0400 (Moscow Standard Time)");

    let dtf = new Intl.DateTimeFormat("en-US", {hour: "numeric", minute: "numeric"});
    assertEq(dtf.format(dt), "12:13 PM");
});
inTimeZone("Asia/Baku", () => {
    let dt = new Date(2012, Month.May, 14, 12, 13, 14);
    assertDateTime(dt, "Mon May 14 2012 12:13:14 GMT+0500 (Azerbaijan Summer Time)");

    let dtf = new Intl.DateTimeFormat("en-US", {hour: "numeric", minute: "numeric"});
    assertEq(dtf.format(dt), "12:13 PM");
});
inTimeZone("Asia/Tbilisi", () => {
    let dt = new Date(2012, Month.May, 14, 12, 13, 14);
    assertDateTime(dt, "Mon May 14 2012 12:13:14 GMT+0400 (Georgia Standard Time)");

    let dtf = new Intl.DateTimeFormat("en-US", {hour: "numeric", minute: "numeric"});
    assertEq(dtf.format(dt), "12:13 PM");
});

// bug 1335818
inTimeZone("Asia/Jerusalem", () => {
    let dt1 = new Date(2013, Month.March, 22, 1, 0, 0, 0);
    assertDateTime(dt1, "Fri Mar 22 2013 01:00:00 GMT+0200 (Israel Standard Time)");

    let dt2 = new Date(2013, Month.March, 22, 2, 0, 0, 0);
    assertDateTime(dt2, "Fri Mar 22 2013 02:00:00 GMT+0200 (Israel Standard Time)");

    let dt3 = new Date(2013, Month.March, 22, 3, 0, 0, 0);
    assertDateTime(dt3, "Fri Mar 22 2013 03:00:00 GMT+0200 (Israel Standard Time)");

    let dt4 = new Date(2013, Month.March, 29, 1, 0, 0, 0);
    assertDateTime(dt4, "Fri Mar 29 2013 01:00:00 GMT+0200 (Israel Standard Time)");

    let dt5 = new Date(2013, Month.March, 29, 2, 0, 0, 0);
    assertDateTime(dt5, "Fri Mar 29 2013 03:00:00 GMT+0300 (Israel Daylight Time)");

    let dt6 = new Date(2013, Month.March, 29, 3, 0, 0, 0);
    assertDateTime(dt6, "Fri Mar 29 2013 03:00:00 GMT+0300 (Israel Daylight Time)");
});

// bug 1342853
inTimeZone("America/Sao_Paulo", () => {
    let dt1 = new Date(2017, Month.October, 14, 12, 0, 0);
    assertDateTime(dt1, "Sat Oct 14 2017 12:00:00 GMT-0300 (Brasilia Standard Time)");
    assertEq(dt1.getTimezoneOffset(), 180);

    let dt2 = new Date(2017, Month.October, 15, 12, 0, 0);
    assertDateTime(dt2, "Sun Oct 15 2017 12:00:00 GMT-0200 (Brasilia Summer Time)");
    assertEq(dt2.getTimezoneOffset(), 120);

    let dt3 = new Date(2018, Month.February, 17, 12, 0, 0);
    assertDateTime(dt3, "Sat Feb 17 2018 12:00:00 GMT-0200 (Brasilia Summer Time)");
    assertEq(dt3.getTimezoneOffset(), 120);

    let dt4 = new Date(2018, Month.February, 18, 12, 0, 0);
    assertDateTime(dt4, "Sun Feb 18 2018 12:00:00 GMT-0300 (Brasilia Standard Time)");
    assertEq(dt4.getTimezoneOffset(), 180);

    let dt5 = new Date(2018, Month.November, 3, 12, 0, 0);
    assertDateTime(dt5, "Sat Nov 03 2018 12:00:00 GMT-0300 (Brasilia Standard Time)");
    assertEq(dt5.getTimezoneOffset(), 180);

    let dt6 = new Date(2018, Month.November, 4, 12, 0, 0);
    assertDateTime(dt6, "Sun Nov 04 2018 12:00:00 GMT-0200 (Brasilia Summer Time)");
    assertEq(dt6.getTimezoneOffset(), 120);

    let dt7 = new Date(2019, Month.February, 16, 12, 0, 0);
    assertDateTime(dt7, "Sat Feb 16 2019 12:00:00 GMT-0200 (Brasilia Summer Time)");
    assertEq(dt7.getTimezoneOffset(), 120);

    let dt8 = new Date(2019, Month.February, 17, 12, 0, 0);
    assertDateTime(dt8, "Sun Feb 17 2019 12:00:00 GMT-0300 (Brasilia Standard Time)");
    assertEq(dt8.getTimezoneOffset(), 180);
});

// bug 1365192
inTimeZone("America/Asuncion", () => {
    let dt = new Date(2018, Month.March, 31);
    assertDateTime(dt, "Sat Mar 31 2018 00:00:00 GMT-0400 (Paraguay Standard Time)");
    assertEq(dt.getTimezoneOffset(), 240);
});

// bug 1385643
inTimeZone("Europe/Warsaw", () => {
    let shortMonths = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"];
    let dtf = new Intl.DateTimeFormat("en-US", {month: "short"});

    for (let year = 1900; year <= 2100; ++year) {
        for (let month = Month.January; month <= Month.December; ++month) {
            let date = new Date(year, month, 1);
            assertEq(dtf.format(date), shortMonths[month]);
        }
    }
});

// bug 1401696
inTimeZone("Europe/Berlin", () => {
    let dt = new Date(1970, Month.January, 1, 0, 0, 0);
    assertDateTime(dt, "Thu Jan 01 1970 00:00:00 GMT+0100 (Central European Standard Time)");
});

// bug 1459842
inTimeZone("Europe/Moscow", () => {
    let dt1 = new Date(1004, Month.April, 1, 2, 0, 0);
    assertDateTime(dt1, "Sun Apr 01 1004 02:00:00 GMT+0230 (Moscow Standard Time)");
    assertEq(dt1.getHours(), 2);

    let dt2 = new Date(1004, Month.April, 1, 1, 0, 0);
    assertDateTime(dt2, "Sun Apr 01 1004 01:00:00 GMT+0230 (Moscow Standard Time)");
    assertEq(dt2.getHours(), 1);

    let dt3 = new Date(1004, Month.April, 1, 0, 0, 0);
    assertDateTime(dt3, "Sun Apr 01 1004 00:00:00 GMT+0230 (Moscow Standard Time)");
    assertEq(dt3.getHours(), 0);

    let dt4 = new Date(1004, Month.March, 1, 2, 0, 0);
    assertDateTime(dt4, "Thu Mar 01 1004 02:00:00 GMT+0230 (Moscow Standard Time)");
    assertEq(dt4.getHours(), 2);

    let dt5 = new Date(1004, Month.March, 1, 1, 0, 0);
    assertDateTime(dt5, "Thu Mar 01 1004 01:00:00 GMT+0230 (Moscow Standard Time)");
    assertEq(dt5.getHours(), 1);

    let dt6 = new Date(1004, Month.March, 1, 0, 0, 0);
    assertDateTime(dt6, "Thu Mar 01 1004 00:00:00 GMT+0230 (Moscow Standard Time)");
    assertEq(dt6.getHours(), 0);

});

// bug 1799638
inTimeZone("Asia/Tokyo", () => {
  let dt1 = new Date(1948, Month.May, 1, 23, 59, 59);
  assertDateTime(dt1, "Sat May 01 1948 23:59:59 GMT+0900 (Japan Standard Time)");
  assertEq(dt1.getTimezoneOffset(), -540);

  let dt2 = new Date(1948, Month.May, 1, 24, 0, 0);
  assertDateTime(dt2, "Sun May 02 1948 01:00:00 GMT+1000 (Japan Daylight Time)");
  assertEq(dt2.getTimezoneOffset(), -600);

  let dt3 = new Date(1948, Month.May, 1, 23, 59, 59, 1);
  assertDateTime(dt3, "Sat May 01 1948 23:59:59 GMT+0900 (Japan Standard Time)");
  assertEq(dt3.getTimezoneOffset(), -540);
});
inTimeZone("America/New_York", () => {
  let dt1 = new Date(1966, Month.April, 24, 1, 59, 59);
  assertDateTime(dt1, "Sun Apr 24 1966 01:59:59 GMT-0500 (Eastern Standard Time)");
  assertEq(dt1.getTimezoneOffset(), 300);

  let dt2 = new Date(1966, Month.April, 24, 2, 0, 0);
  assertDateTime(dt2, "Sun Apr 24 1966 03:00:00 GMT-0400 (Eastern Daylight Time)");
  assertEq(dt2.getTimezoneOffset(), 240);

  let dt3 = new Date(1966, Month.April, 24, 1, 59, 59, 1);
  assertDateTime(dt3, "Sun Apr 24 1966 01:59:59 GMT-0500 (Eastern Standard Time)");
  assertEq(dt3.getTimezoneOffset(), 300);
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
