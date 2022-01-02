// |reftest| skip-if(xulRuntime.OS=="WINNT") -- Windows doesn't accept IANA names for the TZ env variable

// bug 158328
inTimeZone("Europe/London", () => {
    let dt1 = new Date(2002, Month.July, 19, 16, 10, 55);
    assertDateTime(dt1, "Fri Jul 19 2002 16:10:55 GMT+0100 (BST)", "British Summer Time");

    let dt2 = new Date(2009, Month.December, 24, 13, 44, 52);
    assertDateTime(dt2, "Thu Dec 24 2009 13:44:52 GMT+0000 (GMT)", "Greenwich Mean Time");
});

// bug 294908
inTimeZone("America/New_York", () => {
    let dt = new Date(2003, Month.April, 6, 2, 30, 00);
    assertDateTime(dt, "Sun Apr 06 2003 03:30:00 GMT-0400 (EDT)", "Eastern Daylight Time");
});

// bug 610183
inTimeZone("America/Los_Angeles", () => {
    let dt = new Date(2014, Month.November, 2, 1, 47, 42);
    assertDateTime(dt, "Sun Nov 02 2014 01:47:42 GMT-0700 (PDT)", "Pacific Daylight Time");
});

// bug 629465
inTimeZone("America/Denver", () => {
    let dt1 = new Date(Date.UTC(2015, Month.November, 1, 0, 0, 0) + 6 * msPerHour);
    assertDateTime(dt1, "Sun Nov 01 2015 00:00:00 GMT-0600 (MDT)", "Mountain Daylight Time");

    let dt2 = new Date(Date.UTC(2015, Month.November, 1, 1, 0, 0) + 6 * msPerHour);
    assertDateTime(dt2, "Sun Nov 01 2015 01:00:00 GMT-0600 (MDT)", "Mountain Daylight Time");

    let dt3 = new Date(Date.UTC(2015, Month.November, 1, 1, 0, 0) + 7 * msPerHour);
    assertDateTime(dt3, "Sun Nov 01 2015 01:00:00 GMT-0700 (MST)", "Mountain Standard Time");
});

// bug 637244
inTimeZone("Europe/Helsinki", () => {
    let dt1 = new Date(2016, Month.March, 27, 2, 59);
    assertDateTime(dt1, "Sun Mar 27 2016 02:59:00 GMT+0200 (EET)", "Eastern European Standard Time");

    let dt2 = new Date(2016, Month.March, 27, 3, 0);
    assertDateTime(dt2, "Sun Mar 27 2016 04:00:00 GMT+0300 (EEST)", "Eastern European Summer Time");
});

// bug 718175
inTimeZone("Europe/London", () => {
    let dt = new Date(0);
    assertEq(dt.getHours(), 1);
});

// bug 719274
inTimeZone("Pacific/Auckland", () => {
    let dt = new Date(2012, Month.January, 19, 12, 54, 27);
    assertDateTime(dt, "Thu Jan 19 2012 12:54:27 GMT+1300 (NZDT)", "New Zealand Daylight Time");
});

// bug 742427
inTimeZone("Europe/Paris", () => {
    let dt1 = new Date(2009, Month.March, 29, 1, 0, 0);
    assertDateTime(dt1, "Sun Mar 29 2009 01:00:00 GMT+0100 (CET)", "Central European Standard Time");
    dt1.setHours(dt1.getHours() + 1);
    assertDateTime(dt1, "Sun Mar 29 2009 03:00:00 GMT+0200 (CEST)", "Central European Summer Time");

    let dt2 = new Date(2010, Month.March, 29, 1, 0, 0);
    assertDateTime(dt2, "Mon Mar 29 2010 01:00:00 GMT+0200 (CEST)", "Central European Summer Time");
    dt2.setHours(dt2.getHours() + 1);
    assertDateTime(dt2, "Mon Mar 29 2010 02:00:00 GMT+0200 (CEST)", "Central European Summer Time");
});
inTimeZone("America/New_York", () => {
    let dt = new Date(2009, Month.March, 8, 1, 0, 0);
    assertDateTime(dt, "Sun Mar 08 2009 01:00:00 GMT-0500 (EST)", "Eastern Standard Time");
    dt.setHours(dt.getHours() + 1);
    assertDateTime(dt, "Sun Mar 08 2009 03:00:00 GMT-0400 (EDT)", "Eastern Daylight Time");
});
inTimeZone("America/Denver", () => {
    let dt = new Date(2009, Month.March, 8, 1, 0, 0);
    assertDateTime(dt, "Sun Mar 08 2009 01:00:00 GMT-0700 (MST)", "Mountain Standard Time");
    dt.setHours(dt.getHours() + 1);
    assertDateTime(dt, "Sun Mar 08 2009 03:00:00 GMT-0600 (MDT)", "Mountain Daylight Time");
});
inTimeZone("America/New_York", () => {
    let dt1 = new Date(Date.UTC(2008, Month.March, 9, 0, 0, 0) + 5 * msPerHour);
    assertDateTime(dt1, "Sun Mar 09 2008 00:00:00 GMT-0500 (EST)", "Eastern Standard Time");

    let dt2 = new Date(Date.UTC(2008, Month.March, 9, 1, 0, 0) + 5 * msPerHour);
    assertDateTime(dt2, "Sun Mar 09 2008 01:00:00 GMT-0500 (EST)", "Eastern Standard Time");

    let dt3 = new Date(Date.UTC(2008, Month.March, 9, 4, 0, 0) + 4 * msPerHour);
    assertDateTime(dt3, "Sun Mar 09 2008 04:00:00 GMT-0400 (EDT)", "Eastern Daylight Time");
});
inTimeZone("Europe/Paris", () => {
    let dt1 = new Date(Date.UTC(2008, Month.March, 30, 0, 0, 0) - 1 * msPerHour);
    assertDateTime(dt1, "Sun Mar 30 2008 00:00:00 GMT+0100 (CET)", "Central European Standard Time");

    let dt2 = new Date(Date.UTC(2008, Month.March, 30, 1, 0, 0) - 1 * msPerHour);
    assertDateTime(dt2, "Sun Mar 30 2008 01:00:00 GMT+0100 (CET)", "Central European Standard Time");

    let dt3 = new Date(Date.UTC(2008, Month.March, 30, 3, 0, 0) - 2 * msPerHour);
    assertDateTime(dt3, "Sun Mar 30 2008 03:00:00 GMT+0200 (CEST)", "Central European Summer Time");

    let dt4 = new Date(Date.UTC(2008, Month.March, 30, 4, 0, 0) - 2 * msPerHour);
    assertDateTime(dt4, "Sun Mar 30 2008 04:00:00 GMT+0200 (CEST)", "Central European Summer Time");
});

// bug 802627
inTimeZone("America/New_York", () => {
    let dt = new Date(0);
    assertDateTime(dt, "Wed Dec 31 1969 19:00:00 GMT-0500 (EST)", "Eastern Standard Time");
});

// bug 819820
inTimeZone("Europe/London", () => {
    let dt1 = new Date(Date.UTC(2012, Month.October, 28, 0, 59, 59));
    assertDateTime(dt1, "Sun Oct 28 2012 01:59:59 GMT+0100 (BST)", "British Summer Time");

    let dt2 = new Date(Date.UTC(2012, Month.October, 28, 1, 0, 0));
    assertDateTime(dt2, "Sun Oct 28 2012 01:00:00 GMT+0000 (GMT)", "Greenwich Mean Time");

    let dt3 = new Date(Date.UTC(2012, Month.October, 28, 1, 59, 59));
    assertDateTime(dt3, "Sun Oct 28 2012 01:59:59 GMT+0000 (GMT)", "Greenwich Mean Time");

    let dt4 = new Date(Date.UTC(2012, Month.October, 28, 2, 0, 0));
    assertDateTime(dt4, "Sun Oct 28 2012 02:00:00 GMT+0000 (GMT)", "Greenwich Mean Time");
});

// bug 879261
inTimeZone("America/New_York", () => {
    let dt1 = new Date(1362891600000);
    assertDateTime(dt1, "Sun Mar 10 2013 00:00:00 GMT-0500 (EST)", "Eastern Standard Time");

    let dt2 = new Date(dt1.setHours(dt1.getHours() + 24));
    assertDateTime(dt2, "Mon Mar 11 2013 00:00:00 GMT-0400 (EDT)", "Eastern Daylight Time");
});
inTimeZone("America/Los_Angeles", () => {
    let dt1 = new Date(2014, Month.January, 1);
    assertDateTime(dt1, "Wed Jan 01 2014 00:00:00 GMT-0800 (PST)", "Pacific Standard Time");

    let dt2 = new Date(2014, Month.August, 1);
    assertDateTime(dt2, "Fri Aug 01 2014 00:00:00 GMT-0700 (PDT)", "Pacific Daylight Time");
});
inTimeZone("America/New_York", () => {
    let dt1 = new Date(2016, Month.October, 14, 3, 5, 9);
    assertDateTime(dt1, "Fri Oct 14 2016 03:05:09 GMT-0400 (EDT)", "Eastern Daylight Time");

    let dt2 = new Date(2016, Month.January, 9, 23, 26, 40);
    assertDateTime(dt2, "Sat Jan 09 2016 23:26:40 GMT-0500 (EST)", "Eastern Standard Time");
});

// bug 994086
inTimeZone("Europe/Vienna", () => {
    let dt1 = new Date(2014, Month.March, 30, 2, 0);
    assertDateTime(dt1, "Sun Mar 30 2014 03:00:00 GMT+0200 (CEST)", "Central European Summer Time");

    let dt2 = new Date(2014, Month.March, 30, 3, 0);
    assertDateTime(dt2, "Sun Mar 30 2014 03:00:00 GMT+0200 (CEST)", "Central European Summer Time");

    let dt3 = new Date(2014, Month.March, 30, 4, 0);
    assertDateTime(dt3, "Sun Mar 30 2014 04:00:00 GMT+0200 (CEST)", "Central European Summer Time");
});

// bug 1084434
inTimeZone("America/Sao_Paulo", () => {
    let dt = new Date(2014, Month.October, 19);
    assertEq(dt.getDate(), 19);
    assertEq(dt.getHours(), 1);
    assertDateTime(dt, "Sun Oct 19 2014 01:00:00 GMT-0200 (BRST)", "-02", "Brasilia Summer Time");
});

// bug 1084547
inTimeZone("America/New_York", () => {
    let dt = new Date(Date.parse("2014-11-02T02:00:00-04:00"));
    assertDateTime(dt, "Sun Nov 02 2014 01:00:00 GMT-0500 (EST)", "Eastern Standard Time");

    dt.setMilliseconds(0);
    assertDateTime(dt, "Sun Nov 02 2014 01:00:00 GMT-0400 (EDT)", "Eastern Daylight Time");
});

// bug 1118690
inTimeZone("Europe/London", () => {
    let dt = new Date(1965, Month.January, 1);
    assertEq(dt.getFullYear(), 1965);
});

// bug 1155096
inTimeZone("Europe/Moscow", () => {
    let dt1 = new Date(1981, Month.March, 32);
    assertEq(dt1.getDate(), 1);

    let dt2 = new Date(1982, Month.March, 32);
    assertEq(dt2.getDate(), 1);

    let dt3 = new Date(1983, Month.March, 32);
    assertEq(dt3.getDate(), 1);

    let dt4 = new Date(1984, Month.March, 32);
    assertEq(dt4.getDate(), 1);
});

// bug 1284507
inTimeZone("Atlantic/Azores", () => {
    let dt1 = new Date(2017, Month.March, 25, 0, 0, 0);
    assertDateTime(dt1, "Sat Mar 25 2017 00:00:00 GMT-0100 (AZOT)", "-01", "Azores Standard Time");

    let dt2 = new Date(2016, Month.October, 30, 0, 0, 0);
    assertDateTime(dt2, "Sun Oct 30 2016 00:00:00 GMT+0000 (AZOST)", "+00", "Azores Summer Time");

    let dt3 = new Date(2016, Month.October, 30, 23, 0, 0);
    assertDateTime(dt3, "Sun Oct 30 2016 23:00:00 GMT-0100 (AZOT)", "-01", "Azores Standard Time");
});

// bug 1303306
inTimeZone("America/New_York", () => {
    let dt = new Date(2016, Month.September, 15, 16, 14, 48);
    assertDateTime(dt, "Thu Sep 15 2016 16:14:48 GMT-0400 (EDT)", "Eastern Daylight Time");
});

// bug 1317364
inTimeZone("America/Los_Angeles", () => {
    let dt = new Date(2016, Month.March, 13, 2, 30, 0, 0);
    assertDateTime(dt, "Sun Mar 13 2016 03:30:00 GMT-0700 (PDT)", "Pacific Daylight Time");

    let dt2 = new Date(2016, Month.January, 5, 0, 30, 30, 500);
    assertDateTime(dt2, "Tue Jan 05 2016 00:30:30 GMT-0800 (PST)", "Pacific Standard Time");

    let dt3 = new Date(dt2.getTime());
    dt3.setMonth(dt2.getMonth() + 2);
    dt3.setDate(dt2.getDate() + 7 + 1);
    dt3.setHours(dt2.getHours() + 2);

    assertEq(dt3.getHours(), 3);
});

// bug 1335818
inTimeZone("Asia/Jerusalem", () => {
    let dt1 = new Date(2013, Month.March, 22, 1, 0, 0, 0);
    assertDateTime(dt1, "Fri Mar 22 2013 01:00:00 GMT+0200 (IST)", "Israel Standard Time");

    let dt2 = new Date(2013, Month.March, 22, 2, 0, 0, 0);
    assertDateTime(dt2, "Fri Mar 22 2013 02:00:00 GMT+0200 (IST)", "Israel Standard Time");

    let dt3 = new Date(2013, Month.March, 22, 3, 0, 0, 0);
    assertDateTime(dt3, "Fri Mar 22 2013 03:00:00 GMT+0200 (IST)", "Israel Standard Time");

    let dt4 = new Date(2013, Month.March, 29, 1, 0, 0, 0);
    assertDateTime(dt4, "Fri Mar 29 2013 01:00:00 GMT+0200 (IST)", "Israel Standard Time");

    let dt5 = new Date(2013, Month.March, 29, 2, 0, 0, 0);
    assertDateTime(dt5, "Fri Mar 29 2013 03:00:00 GMT+0300 (IDT)", "Israel Daylight Time");

    let dt6 = new Date(2013, Month.March, 29, 3, 0, 0, 0);
    assertDateTime(dt6, "Fri Mar 29 2013 03:00:00 GMT+0300 (IDT)", "Israel Daylight Time");
});

// bug 1355272
inTimeZone("America/Los_Angeles", () => {
    let dt = new Date(2017, Month.April, 10, 17, 25, 07);
    assertDateTime(dt, "Mon Apr 10 2017 17:25:07 GMT-0700 (PDT)", "Pacific Daylight Time");
});

if (typeof reportCompare === "function")
    reportCompare(true, true);
