// |reftest| skip-if(xulRuntime.OS=="WINNT"||!this.hasOwnProperty("Intl")) -- Windows doesn't accept IANA names for the TZ env variable; Requires ICU time zone support

// Imported tests from es6draft and then adapted to use ICU/CLDR time zone display names.

function assertSame(expected, actual, message = undefined) {
  if (message !== undefined)
    assertEq(actual, expected, String(message));
  else
    assertEq(actual, expected);
}

function assertTrue(actual, message = undefined) {
  assertSame(true, actual, message);
}

// File: lib/datetime.jsm
const {
  DayOfWeek,
  Month,
  DateTime,
  TimeZone,
  Format,
} = (function() {

// 5.2 Algorithm Conventions
function modulo(dividend, divisor) {
  assertTrue(typeof dividend === "number");
  assertTrue(typeof divisor === "number");
  assertTrue(divisor !== 0 && Number.isFinite(divisor));
  let remainder = dividend % divisor;
  // NB: add +0 to convert -0 to +0
  return (remainder >= 0 ? remainder + 0 : remainder + divisor);
}

// 7.1.4 ToInteger ( argument )
function ToInteger(number) {
  /* steps 1-2 */
  assertTrue(typeof number === "number");
  /* step 3 */
  if (Number.isNaN(number))
    return +0.0;
  /* step 4 */
  if (number == 0.0 || !Number.isFinite(number))
    return number;
  /* step 5 */
  return Math.sign(number) * Math.floor(Math.abs(number));
}

// 20.3.1.2 Day Number and Time within Day
const msPerDay = 86400000;

// 20.3.1.2 Day Number and Time within Day
function Day(t) {
  assertTrue(typeof t === "number");
  return Math.floor(t / msPerDay);
}

// 20.3.1.2 Day Number and Time within Day
function TimeWithinDay(t) {
  assertTrue(typeof t === "number");
  return modulo(t, msPerDay);
}

// 20.3.1.3 Year Number
function DaysInYear(y) {
  assertTrue(typeof y === "number");
  if (y % 4 !== 0) {
    return 365;
  }
  if (y % 100 !== 0) {
    return 366;
  }
  if (y % 400 !== 0) {
    return 365;
  }
  return 366;
}

// 20.3.1.3 Year Number
function DayFromYear(y) {
  assertTrue(typeof y === "number");
  return 365 * (y - 1970) + Math.floor((y - 1969) / 4) - Math.floor((y - 1901) / 100) + Math.floor((y - 1601) / 400);
}

// 20.3.1.3 Year Number
function TimeFromYear(y) {
  assertTrue(typeof y === "number");
  return msPerDay * DayFromYear(y);
}

// TODO: fill in rest

// 20.3.1.10 Hours, Minutes, Second, and Milliseconds
const HoursPerDay = 24;
const MinutesPerHour = 60;
const SecondsPerMinute = 60;
const msPerSecond = 1000;
const msPerMinute = msPerSecond * SecondsPerMinute;
const msPerHour = msPerMinute * MinutesPerHour;

// 20.3.1.10 Hours, Minutes, Second, and Milliseconds
function HourFromTime(t) {
  assertTrue(typeof t === "number");
  return modulo(Math.floor(t / msPerHour), HoursPerDay);
}

// 20.3.1.10 Hours, Minutes, Second, and Milliseconds
function MinFromTime(t) {
  assertTrue(typeof t === "number");
  return modulo(Math.floor(t / msPerMinute), MinutesPerHour);
}

// 20.3.1.10 Hours, Minutes, Second, and Milliseconds
function SecFromTime(t) {
  assertTrue(typeof t === "number");
  return modulo(Math.floor(t / msPerSecond), SecondsPerMinute);
}

// 20.3.1.10 Hours, Minutes, Second, and Milliseconds
function msFromTime(t) {
  assertTrue(typeof t === "number");
  return modulo(t, msPerSecond);
}

// 20.3.1.11 MakeTime (hour, min, sec, ms)
function MakeTime(hour, min, sec, ms) {
  assertTrue(typeof hour === "number");
  assertTrue(typeof min === "number");
  assertTrue(typeof sec === "number");
  assertTrue(typeof ms === "number");
  if (!Number.isFinite(hour) || !Number.isFinite(min) || !Number.isFinite(sec) || !Number.isFinite(ms)) {
    return Number.NaN;
  }
  let h = ToInteger(hour);
  let m = ToInteger(min);
  let s = ToInteger(sec);
  let milli = ToInteger(ms);
  let t = h * msPerHour + m * msPerMinute + s * msPerSecond + milli;
  return t;
}

// 20.3.1.12 MakeDay (year, month, date)
function MakeDay(year, month, date) {
  assertTrue(typeof year === "number");
  assertTrue(typeof month === "number");
  assertTrue(typeof date === "number");
  if (!Number.isFinite(year) || !Number.isFinite(month) || !Number.isFinite(date)) {
    return Number.NaN;
  }
  let y = ToInteger(year);
  let m = ToInteger(month);
  let dt = ToInteger(date);
  let ym = y + Math.floor(m / 12);
  let mn = modulo(m, 12);

  const monthStart = [0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334];
  let day = Math.floor(TimeFromYear(ym) / msPerDay) + monthStart[mn];
  if (mn >= 2 && DaysInYear(ym) == 366) {
    day += 1;
  }

  return day + dt - 1;
}

// 20.3.1.13 MakeDate (day, time)
function MakeDate(day, time) {
  assertTrue(typeof day === "number");
  assertTrue(typeof time === "number");
  if (!Number.isFinite(day) || !Number.isFinite(time)) {
    return Number.NaN;
  }
  return day * msPerDay + time;
}

// 20.3.1.14 TimeClip (time)
function TimeClip(time) {
  assertTrue(typeof time === "number");
  if (!Number.isFinite(time)) {
    return Number.NaN;
  }
  if (Math.abs(time) > 8.64e15) {
    return Number.NaN;
  }
  return ToInteger(time) + (+0);
}

const DayOfWeek = {
  Sunday: 0,
  Monday: 1,
  Tuesday: 2,
  Wednesday: 3,
  Thursday: 4,
  Friday: 5,
  Saturday: 6,
};

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

const DateTime = {
  Local: class {
    constructor(year, month, day, weekday, hour = 0, minute = 0, second = 0, ms = 0) {
      Object.assign(this, {year, month, day, weekday, hour, minute, second, ms});
    }

    toDate() {
      return new Date(this.year, this.month, this.day, this.hour, this.minute, this.second, this.ms);
    }
  },
  UTC: class {
    constructor(year, month, day, weekday, hour = 0, minute = 0, second = 0, ms = 0) {
      Object.assign(this, {year, month, day, weekday, hour, minute, second, ms});
    }

    toInstant() {
      return MakeDate(MakeDay(this.year, this.month, this.day), MakeTime(this.hour, this.minute, this.second, this.ms));
    }
  },
};

function TimeZone(hour, minute = 0, second = 0) {
  return new class TimeZone {
    constructor(hour, minute, second) {
      Object.assign(this, {hour, minute, second});
    }

    toOffset() {
      let offset = TimeZoneOffset(this.hour, this.minute, this.second);
      return offset !== 0 ? -offset : 0;
    }
  }(hour, minute, second);

  function TimeZoneOffset(hour, minute = 0, second = 0) {
    assertTrue(typeof hour === "number");
    assertTrue(typeof minute === "number");
    assertTrue(typeof second === "number");
    assertTrue(minute >= 0);
    assertTrue(second >= 0);
    if (hour < 0 || Object.is(-0, hour)) {
      return hour * MinutesPerHour - minute - (second / 60);
    }
    return hour * MinutesPerHour + minute + (second / 60);
  }
}

const Format = {
  Locale: "en-US",
  DateTime: {
    localeMatcher: "lookup",
    timeZone: void 0,
    weekday: "short",
    era: void 0,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    timeZoneName: "short",
    formatMatcher: "best fit",
    hour12: void 0,
  },
  Date: {
    localeMatcher: "lookup",
    timeZone: void 0,
    weekday: "short",
    era: void 0,
    year: "numeric",
    month: "2-digit",
    day: "2-digit",
    hour: void 0,
    minute: void 0,
    second: void 0,
    timeZoneName: void 0,
    formatMatcher: "best fit",
    hour12: void 0,
  },
  Time: {
    localeMatcher: "lookup",
    timeZone: void 0,
    weekday: void 0,
    era: void 0,
    year: void 0,
    month: void 0,
    day: void 0,
    hour: "2-digit",
    minute: "2-digit",
    second: "2-digit",
    timeZoneName: "short",
    formatMatcher: "best fit",
    hour12: void 0,
  },
};

return {
  DayOfWeek,
  Month,
  DateTime,
  TimeZone,
  Format,
};
})();


// File: lib/assert-datetime.js

function assertDate(local, utc, timeZone, options, formatArgs) {
  let d = local.toDate();
  assertDateValue(d, utc.toInstant(), timeZone.toOffset());
  assertLocalDate(d, local);
  assertUTCDate(d, utc);
  assertDateString(d, options, formatArgs);
}

function assertDateValue(actual, dateValue, timeZoneOffset) {
  assertSame(dateValue, actual.valueOf(), `valueOf()[${dateValue - actual.valueOf()}]`);
  assertSame(dateValue, actual.getTime(), `valueOf()[${dateValue - actual.getTime()}]`);
  assertSame(timeZoneOffset, actual.getTimezoneOffset(), "getTimezoneOffset()");
}

function assertLocalDate(actual, {year, month, day, weekday, hour = 0, minute = 0, second = 0, ms = 0}) {
  assertSame(year, actual.getFullYear(), "getFullYear()");
  assertSame(month, actual.getMonth(), "getMonth()");
  assertSame(day, actual.getDate(), "getDate()");
  assertSame(weekday, actual.getDay(), "getDay()");
  assertSame(hour, actual.getHours(), "getHours()");
  assertSame(minute, actual.getMinutes(), "getMinutes()");
  assertSame(second, actual.getSeconds(), "getSeconds()");
  assertSame(ms, actual.getMilliseconds(), "getMilliseconds()");
}

function assertUTCDate(actual, {year, month, day, weekday, hour = 0, minute = 0, second = 0, ms = 0}) {
  assertSame(year, actual.getUTCFullYear(), "getUTCFullYear()");
  assertSame(month, actual.getUTCMonth(), "getUTCMonth()");
  assertSame(day, actual.getUTCDate(), "getUTCDate()");
  assertSame(weekday, actual.getUTCDay(), "getUTCDay()");
  assertSame(hour, actual.getUTCHours(), "getUTCHours()");
  assertSame(minute, actual.getUTCMinutes(), "getUTCMinutes()");
  assertSame(second, actual.getUTCSeconds(), "getUTCSeconds()");
  assertSame(ms, actual.getUTCMilliseconds(), "getUTCMilliseconds()");
}

function assertDateString(actual, options, formatArgs = {
  LocaleString: [Format.Locale, Format.DateTime],
  LocaleDateString: [Format.Locale, Format.Date],
  LocaleTimeString: [Format.Locale, Format.Time],
}) {
  for (var key of Object.keys(options)) {
    var args = formatArgs[key] || [];
    assertSame(options[key], actual[`to${key}`](...args), `to${key}()`);
  }
}


// File: Date/Africa_Monrovia.js
// Liberia was the last country to switch to UTC based offsets (1972 May).

inTimeZone("Africa/Monrovia", () => {
{
  let local = new DateTime.Local(1972, Month.January, 6, DayOfWeek.Thursday, 0, 0, 0);
  let utc = new DateTime.UTC(1972, Month.January, 6, DayOfWeek.Thursday, 0, 44, 30);

  assertDate(local, utc, TimeZone(-0,44,30), {
    String: "Thu Jan 06 1972 00:00:00 GMT-0044 (Greenwich Mean Time)",
    UTCString: "Thu, 06 Jan 1972 00:44:30 GMT",
  });
}

{
  let local = new DateTime.Local(1972, Month.January, 6, DayOfWeek.Thursday, 23, 59, 0);
  let utc = new DateTime.UTC(1972, Month.January, 7, DayOfWeek.Friday, 0, 43, 30);

  assertDate(local, utc, TimeZone(-0,44,30), {
    String: "Thu Jan 06 1972 23:59:00 GMT-0044 (Greenwich Mean Time)",
    UTCString: "Fri, 07 Jan 1972 00:43:30 GMT",
  });
}

{
  let local = new DateTime.Local(1972, Month.January, 7, DayOfWeek.Friday, 0, 0, 0);
  let utc = new DateTime.UTC(1972, Month.January, 7, DayOfWeek.Friday, 0, 44, 30);

  assertDateValue(local.toDate(), utc.toInstant(), TimeZone(+0).toOffset());

  assertDateString(local.toDate(), {
    String: "Fri Jan 07 1972 00:44:30 GMT+0000 (Greenwich Mean Time)",
    UTCString: "Fri, 07 Jan 1972 00:44:30 GMT",
  });
}

{
  let local = new DateTime.Local(1972, Month.January, 7, DayOfWeek.Friday, 0, 44, 30);
  let utc = new DateTime.UTC(1972, Month.January, 7, DayOfWeek.Friday, 0, 44, 30);

  assertDate(local, utc, TimeZone(+0), {
    String: "Fri Jan 07 1972 00:44:30 GMT+0000 (Greenwich Mean Time)",
    UTCString: "Fri, 07 Jan 1972 00:44:30 GMT",
  });
}

{
  let local = new DateTime.Local(1972, Month.January, 7, DayOfWeek.Friday, 0, 45, 0);
  let utc = new DateTime.UTC(1972, Month.January, 7, DayOfWeek.Friday, 0, 45, 0);

  assertDate(local, utc, TimeZone(+0), {
    String: "Fri Jan 07 1972 00:45:00 GMT+0000 (Greenwich Mean Time)",
    UTCString: "Fri, 07 Jan 1972 00:45:00 GMT",
  });
}

{
  let local = new DateTime.Local(1972, Month.January, 8, DayOfWeek.Saturday, 0, 0, 0);
  let utc = new DateTime.UTC(1972, Month.January, 8, DayOfWeek.Saturday, 0, 0, 0);

  assertDate(local, utc, TimeZone(+0), {
    String: "Sat Jan 08 1972 00:00:00 GMT+0000 (Greenwich Mean Time)",
    UTCString: "Sat, 08 Jan 1972 00:00:00 GMT",
  });
}
});


// File: Date/Africa_Monrovia.js
// Africa/Tripoli switched from +02:00 to +01:00 and back.

inTimeZone("Africa/Tripoli", () => {
{
  // +02:00 (standard time)
  let local = new DateTime.Local(2012, Month.November, 1, DayOfWeek.Thursday, 0, 0, 0);
  let utc = new DateTime.UTC(2012, Month.October, 31, DayOfWeek.Wednesday, 22, 0, 0);

  assertDate(local, utc, TimeZone(+2), {
    String: "Thu Nov 01 2012 00:00:00 GMT+0200 (Eastern European Standard Time)",
    UTCString: "Wed, 31 Oct 2012 22:00:00 GMT",
  });
}

{
  // +01:00 (standard time)
  let local = new DateTime.Local(2012, Month.December, 1, DayOfWeek.Saturday, 0, 0, 0);
  let utc = new DateTime.UTC(2012, Month.November, 30, DayOfWeek.Friday, 23, 0, 0);

  assertDate(local, utc, TimeZone(+1), {
    String: "Sat Dec 01 2012 00:00:00 GMT+0100 (Eastern European Standard Time)",
    UTCString: "Fri, 30 Nov 2012 23:00:00 GMT",
  });
}

{
  // +01:00 (daylight savings)
  let local = new DateTime.Local(2013, Month.October, 1, DayOfWeek.Tuesday, 0, 0, 0);
  let utc = new DateTime.UTC(2013, Month.September, 30, DayOfWeek.Monday, 22, 0, 0);

  assertDate(local, utc, TimeZone(+2), {
    String: "Tue Oct 01 2013 00:00:00 GMT+0200 (Eastern European Summer Time)",
    UTCString: "Mon, 30 Sep 2013 22:00:00 GMT",
  });
}

{
  // +02:00 (standard time)
  let local = new DateTime.Local(2013, Month.November, 1, DayOfWeek.Friday, 0, 0, 0);
  let utc = new DateTime.UTC(2013, Month.October, 31, DayOfWeek.Thursday, 22, 0, 0);

  assertDate(local, utc, TimeZone(+2), {
    String: "Fri Nov 01 2013 00:00:00 GMT+0200 (Eastern European Standard Time)",
    UTCString: "Thu, 31 Oct 2013 22:00:00 GMT",
  });
}
});


// File: Date/America_Caracas.js
// America/Caracas switched from -04:00 to -04:30 on 2007 Dec 9.

inTimeZone("America/Caracas", () => {
{
  // -04:00 (standard time)
  let local = new DateTime.Local(2007, Month.December, 5, DayOfWeek.Wednesday, 0, 0, 0);
  let utc = new DateTime.UTC(2007, Month.December, 5, DayOfWeek.Wednesday, 4, 0, 0);

  assertDate(local, utc, TimeZone(-4), {
    String: "Wed Dec 05 2007 00:00:00 GMT-0400 (Venezuela Time)",
    DateString: "Wed Dec 05 2007",
    TimeString: "00:00:00 GMT-0400 (Venezuela Time)",
    UTCString: "Wed, 05 Dec 2007 04:00:00 GMT",
    ISOString: "2007-12-05T04:00:00.000Z",
    LocaleString: "Wed, 12/05/2007, 12:00:00 AM GMT-4",
    LocaleDateString: "Wed, 12/05/2007",
    LocaleTimeString: "12:00:00 AM GMT-4",
  });
}

{
  // -04:30 (standard time)
  let local = new DateTime.Local(2007, Month.December, 12, DayOfWeek.Wednesday, 0, 0, 0);
  let utc = new DateTime.UTC(2007, Month.December, 12, DayOfWeek.Wednesday, 4, 30, 0);

  assertDate(local, utc, TimeZone(-4, 30), {
    String: "Wed Dec 12 2007 00:00:00 GMT-0430 (Venezuela Time)",
    DateString: "Wed Dec 12 2007",
    TimeString: "00:00:00 GMT-0430 (Venezuela Time)",
    UTCString: "Wed, 12 Dec 2007 04:30:00 GMT",
    ISOString: "2007-12-12T04:30:00.000Z",
    LocaleString: "Wed, 12/12/2007, 12:00:00 AM GMT-4:30",
    LocaleDateString: "Wed, 12/12/2007",
    LocaleTimeString: "12:00:00 AM GMT-4:30",
  });
}
});


// File: Date/Australia_Lord_Howe.js
// Australia/Lord_Howe time zone offset is +10:30 and daylight savings amount is 00:30.

inTimeZone("Australia/Lord_Howe", () => {
{
  // +10:30 (standard time)
  let local = new DateTime.Local(2010, Month.August, 1, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.July, 31, DayOfWeek.Saturday, 13, 30, 0);

  assertDate(local, utc, TimeZone(+10,30), {
    String: "Sun Aug 01 2010 00:00:00 GMT+1030 (Lord Howe Standard Time)",
    DateString: "Sun Aug 01 2010",
    TimeString: "00:00:00 GMT+1030 (Lord Howe Standard Time)",
    UTCString: "Sat, 31 Jul 2010 13:30:00 GMT",
    ISOString: "2010-07-31T13:30:00.000Z",
    LocaleString: "Sun, 08/01/2010, 12:00:00 AM GMT+10:30",
    LocaleDateString: "Sun, 08/01/2010",
    LocaleTimeString: "12:00:00 AM GMT+10:30",
  });
}

{
  // +10:30 (daylight savings)
  let local = new DateTime.Local(2010, Month.January, 3, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.January, 2, DayOfWeek.Saturday, 13, 0, 0);

  assertDate(local, utc, TimeZone(+11), {
    String: "Sun Jan 03 2010 00:00:00 GMT+1100 (Lord Howe Daylight Time)",
    DateString: "Sun Jan 03 2010",
    TimeString: "00:00:00 GMT+1100 (Lord Howe Daylight Time)",
    UTCString: "Sat, 02 Jan 2010 13:00:00 GMT",
    ISOString: "2010-01-02T13:00:00.000Z",
    LocaleString: "Sun, 01/03/2010, 12:00:00 AM GMT+11",
    LocaleDateString: "Sun, 01/03/2010",
    LocaleTimeString: "12:00:00 AM GMT+11",
  });
}
});


// File: Date/Europe_Amsterdam.js
// Europe/Amsterdam as an example for mean time like timezones after LMT (AMT, NST).
//
// tzdata2022b changed Europe/Amsterdam into a link to Europe/Brussels.

inTimeZone("Europe/Amsterdam", () => {
{
  let local = new DateTime.Local(1935, Month.January, 1, DayOfWeek.Tuesday, 0, 0, 0);
  let utc = new DateTime.UTC(1935, Month.January, 1, DayOfWeek.Tuesday, 0, 0, 0);

  assertDate(local, utc, TimeZone(+0,0,0), {
    String: "Tue Jan 01 1935 00:00:00 GMT+0000 (Central European Standard Time)",
    UTCString: "Tue, 01 Jan 1935 00:00:00 GMT",
  });
}

{
  let local = new DateTime.Local(1935, Month.July, 1, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(1935, Month.June, 30, DayOfWeek.Sunday, 23, 0, 0);

  assertDate(local, utc, TimeZone(+1,0,0), {
    String: "Mon Jul 01 1935 00:00:00 GMT+0100 (Central European Summer Time)",
    UTCString: "Sun, 30 Jun 1935 23:00:00 GMT",
  });
}
});

// Use America/St_Johns as a replacement for the Europe/Amsterdam test case.
//
// Zone America/St_Johns as an example for mean time like timezones after LMT (NST, NDT).

inTimeZone("America/St_Johns", () => {
{
  let local = new DateTime.Local(1917, Month.January, 1, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(1917, Month.January, 1, DayOfWeek.Monday, 3, 30, 52);

  assertDate(local, utc, TimeZone(-3,30,52), {
    String: "Mon Jan 01 1917 00:00:00 GMT-0330 (Newfoundland Standard Time)",
    UTCString: "Mon, 01 Jan 1917 03:30:52 GMT",
  });
}

{
  let local = new DateTime.Local(1917, Month.July, 1, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(1917, Month.July, 1, DayOfWeek.Sunday, 2, 30, 52);

  assertDate(local, utc, TimeZone(-2,30,52), {
    String: "Sun Jul 01 1917 00:00:00 GMT-0230 (Newfoundland Daylight Time)",
    UTCString: "Sun, 01 Jul 1917 02:30:52 GMT",
  });
}
});


// File: Date/Europe_London.js

inTimeZone("Europe/London", () => {
{
  // +01:00 (standard time)
  let local = new DateTime.Local(1970, Month.January, 1, DayOfWeek.Thursday, 0, 0, 0);
  let utc = new DateTime.UTC(1969, Month.December, 31, DayOfWeek.Wednesday, 23, 0, 0);

  assertDate(local, utc, TimeZone(+1), {
    String: "Thu Jan 01 1970 00:00:00 GMT+0100 (Greenwich Mean Time)",
    DateString: "Thu Jan 01 1970",
    TimeString: "00:00:00 GMT+0100 (Greenwich Mean Time)",
    UTCString: "Wed, 31 Dec 1969 23:00:00 GMT",
    ISOString: "1969-12-31T23:00:00.000Z",
    LocaleString: "Thu, 01/01/1970, 12:00:00 AM GMT+1",
    LocaleDateString: "Thu, 01/01/1970",
    LocaleTimeString: "12:00:00 AM GMT+1",
  });
}
});


// File: Date/Europe_Moscow.js

inTimeZone("Europe/Moscow", () => {
{
  let local = new DateTime.Local(1970, Month.January, 1, DayOfWeek.Thursday, 0, 0, 0);
  let utc = new DateTime.UTC(1969, Month.December, 31, DayOfWeek.Wednesday, 21, 0, 0);

  assertDate(local, utc, TimeZone(+3), {
    String: "Thu Jan 01 1970 00:00:00 GMT+0300 (Moscow Standard Time)",
    DateString: "Thu Jan 01 1970",
    TimeString: "00:00:00 GMT+0300 (Moscow Standard Time)",
    UTCString: "Wed, 31 Dec 1969 21:00:00 GMT",
    ISOString: "1969-12-31T21:00:00.000Z",
    LocaleString: "Thu, 01/01/1970, 12:00:00 AM GMT+3",
    LocaleDateString: "Thu, 01/01/1970",
    LocaleTimeString: "12:00:00 AM GMT+3",
  });
}

// Russia was in +02:00 starting on 1991-03-31 until 1992-01-19,
// while still observing DST (transitions 1991-03-31 and 1991-09-29).

{
  // +03:00 (daylight savings)
  let local = new DateTime.Local(1990, Month.September, 1, DayOfWeek.Saturday, 0, 0, 0);
  let utc = new DateTime.UTC(1990, Month.August, 31, DayOfWeek.Friday, 20, 0, 0);

  assertDate(local, utc, TimeZone(+4), {
    String: "Sat Sep 01 1990 00:00:00 GMT+0400 (Moscow Summer Time)",
    DateString: "Sat Sep 01 1990",
    TimeString: "00:00:00 GMT+0400 (Moscow Summer Time)",
    UTCString: "Fri, 31 Aug 1990 20:00:00 GMT",
    ISOString: "1990-08-31T20:00:00.000Z",
    LocaleString: "Sat, 09/01/1990, 12:00:00 AM GMT+4",
    LocaleDateString: "Sat, 09/01/1990",
    LocaleTimeString: "12:00:00 AM GMT+4",
  });
}

{
  // +03:00 (standard time)
  let local = new DateTime.Local(1991, Month.March, 25, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(1991, Month.March, 24, DayOfWeek.Sunday, 21, 0, 0);

  assertDate(local, utc, TimeZone(+3), {
    String: "Mon Mar 25 1991 00:00:00 GMT+0300 (Moscow Standard Time)",
    DateString: "Mon Mar 25 1991",
    TimeString: "00:00:00 GMT+0300 (Moscow Standard Time)",
    UTCString: "Sun, 24 Mar 1991 21:00:00 GMT",
    ISOString: "1991-03-24T21:00:00.000Z",
    LocaleString: "Mon, 03/25/1991, 12:00:00 AM GMT+3",
    LocaleDateString: "Mon, 03/25/1991",
    LocaleTimeString: "12:00:00 AM GMT+3",
  });
}

{
  // +02:00 (daylight savings)
  let local = new DateTime.Local(1991, Month.March, 31, DayOfWeek.Sunday, 12, 0, 0);
  let utc = new DateTime.UTC(1991, Month.March, 31, DayOfWeek.Sunday, 9, 0, 0);

  assertDate(local, utc, TimeZone(+3), {
    String: "Sun Mar 31 1991 12:00:00 GMT+0300 (Moscow Summer Time)",
    DateString: "Sun Mar 31 1991",
    TimeString: "12:00:00 GMT+0300 (Moscow Summer Time)",
    UTCString: "Sun, 31 Mar 1991 09:00:00 GMT",
    ISOString: "1991-03-31T09:00:00.000Z",
    LocaleString: "Sun, 03/31/1991, 12:00:00 PM GMT+3",
    LocaleDateString: "Sun, 03/31/1991",
    LocaleTimeString: "12:00:00 PM GMT+3",
  });
}

{
  // +02:00 (daylight savings)
  let local = new DateTime.Local(1991, Month.September, 28, DayOfWeek.Saturday, 0, 0, 0);
  let utc = new DateTime.UTC(1991, Month.September, 27, DayOfWeek.Friday, 21, 0, 0);

  assertDate(local, utc, TimeZone(+3), {
    String: "Sat Sep 28 1991 00:00:00 GMT+0300 (Moscow Summer Time)",
    DateString: "Sat Sep 28 1991",
    TimeString: "00:00:00 GMT+0300 (Moscow Summer Time)",
    UTCString: "Fri, 27 Sep 1991 21:00:00 GMT",
    ISOString: "1991-09-27T21:00:00.000Z",
    LocaleString: "Sat, 09/28/1991, 12:00:00 AM GMT+3",
    LocaleDateString: "Sat, 09/28/1991",
    LocaleTimeString: "12:00:00 AM GMT+3",
  });
}

{
  // +02:00 (standard time)
  let local = new DateTime.Local(1991, Month.September, 30, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(1991, Month.September, 29, DayOfWeek.Sunday, 22, 0, 0);

  assertDate(local, utc, TimeZone(+2), {
    String: "Mon Sep 30 1991 00:00:00 GMT+0200 (Moscow Standard Time)",
    DateString: "Mon Sep 30 1991",
    TimeString: "00:00:00 GMT+0200 (Moscow Standard Time)",
    UTCString: "Sun, 29 Sep 1991 22:00:00 GMT",
    ISOString: "1991-09-29T22:00:00.000Z",
    LocaleString: "Mon, 09/30/1991, 12:00:00 AM GMT+2",
    LocaleDateString: "Mon, 09/30/1991",
    LocaleTimeString: "12:00:00 AM GMT+2",
  });
}

// Russia stopped observing DST in Oct. 2010 (last transition on 2010-10-31),
// and changed timezone from +03:00 to +04:00 on 2011-03-27.

{
  // +03:00 (daylight savings)
  let local = new DateTime.Local(2010, Month.October, 30, DayOfWeek.Saturday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.October, 29, DayOfWeek.Friday, 20, 0, 0);

  assertDate(local, utc, TimeZone(+4), {
    String: "Sat Oct 30 2010 00:00:00 GMT+0400 (Moscow Summer Time)",
    DateString: "Sat Oct 30 2010",
    TimeString: "00:00:00 GMT+0400 (Moscow Summer Time)",
    UTCString: "Fri, 29 Oct 2010 20:00:00 GMT",
    ISOString: "2010-10-29T20:00:00.000Z",
    LocaleString: "Sat, 10/30/2010, 12:00:00 AM GMT+4",
    LocaleDateString: "Sat, 10/30/2010",
    LocaleTimeString: "12:00:00 AM GMT+4",
  });
}

{
  // +03:00 (standard time)
  let local = new DateTime.Local(2010, Month.November, 1, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.October, 31, DayOfWeek.Sunday, 21, 0, 0);

  assertDate(local, utc, TimeZone(+3), {
    String: "Mon Nov 01 2010 00:00:00 GMT+0300 (Moscow Standard Time)",
    DateString: "Mon Nov 01 2010",
    TimeString: "00:00:00 GMT+0300 (Moscow Standard Time)",
    UTCString: "Sun, 31 Oct 2010 21:00:00 GMT",
    ISOString: "2010-10-31T21:00:00.000Z",
    LocaleString: "Mon, 11/01/2010, 12:00:00 AM GMT+3",
    LocaleDateString: "Mon, 11/01/2010",
    LocaleTimeString: "12:00:00 AM GMT+3",
  });
}

{
  // +04:00 (standard time)
  let local = new DateTime.Local(2011, Month.October, 30, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2011, Month.October, 29, DayOfWeek.Saturday, 20, 0, 0);

  assertDate(local, utc, TimeZone(+4), {
    String: "Sun Oct 30 2011 00:00:00 GMT+0400 (Moscow Standard Time)",
    DateString: "Sun Oct 30 2011",
    TimeString: "00:00:00 GMT+0400 (Moscow Standard Time)",
    UTCString: "Sat, 29 Oct 2011 20:00:00 GMT",
    ISOString: "2011-10-29T20:00:00.000Z",
    LocaleString: "Sun, 10/30/2011, 12:00:00 AM GMT+4",
    LocaleDateString: "Sun, 10/30/2011",
    LocaleTimeString: "12:00:00 AM GMT+4",
  });
}

{
  // +04:00 (standard time)
  let local = new DateTime.Local(2011, Month.November, 1, DayOfWeek.Tuesday, 0, 0, 0);
  let utc = new DateTime.UTC(2011, Month.October, 31, DayOfWeek.Monday, 20, 0, 0);

  assertDate(local, utc, TimeZone(+4), {
    String: "Tue Nov 01 2011 00:00:00 GMT+0400 (Moscow Standard Time)",
    DateString: "Tue Nov 01 2011",
    TimeString: "00:00:00 GMT+0400 (Moscow Standard Time)",
    UTCString: "Mon, 31 Oct 2011 20:00:00 GMT",
    ISOString: "2011-10-31T20:00:00.000Z",
    LocaleString: "Tue, 11/01/2011, 12:00:00 AM GMT+4",
    LocaleDateString: "Tue, 11/01/2011",
    LocaleTimeString: "12:00:00 AM GMT+4",
  });
}

// Russia changed timezone from +04:00 to +03:00 on 2014-10-26.

{
  // +04:00 (standard time)
  let local = new DateTime.Local(2014, Month.October, 26, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2014, Month.October, 25, DayOfWeek.Saturday, 20, 0, 0);

  assertDate(local, utc, TimeZone(+4), {
    String: "Sun Oct 26 2014 00:00:00 GMT+0400 (Moscow Standard Time)",
    DateString: "Sun Oct 26 2014",
    TimeString: "00:00:00 GMT+0400 (Moscow Standard Time)",
    UTCString: "Sat, 25 Oct 2014 20:00:00 GMT",
    ISOString: "2014-10-25T20:00:00.000Z",
    LocaleString: "Sun, 10/26/2014, 12:00:00 AM GMT+4",
    LocaleDateString: "Sun, 10/26/2014",
    LocaleTimeString: "12:00:00 AM GMT+4",
  });
}

{
  // +03:00 (standard time)
  let local = new DateTime.Local(2014, Month.October, 27, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(2014, Month.October, 26, DayOfWeek.Sunday, 21, 0, 0);

  assertDate(local, utc, TimeZone(+3), {
    String: "Mon Oct 27 2014 00:00:00 GMT+0300 (Moscow Standard Time)",
    DateString: "Mon Oct 27 2014",
    TimeString: "00:00:00 GMT+0300 (Moscow Standard Time)",
    UTCString: "Sun, 26 Oct 2014 21:00:00 GMT",
    ISOString: "2014-10-26T21:00:00.000Z",
    LocaleString: "Mon, 10/27/2014, 12:00:00 AM GMT+3",
    LocaleDateString: "Mon, 10/27/2014",
    LocaleTimeString: "12:00:00 AM GMT+3",
  });
}
});


// File: Date/Pacific_Apia.js
// Pacific/Apia switched from -11:00 to +13:00 on 2011 Dec 29 24:00.

inTimeZone("Pacific/Apia", () => {
{
  // -11:00 (daylight savings)
  let local = new DateTime.Local(2011, Month.December, 29, DayOfWeek.Thursday, 0, 0, 0);
  let utc = new DateTime.UTC(2011, Month.December, 29, DayOfWeek.Thursday, 10, 0, 0);

  assertDate(local, utc, TimeZone(-10), {
    String: "Thu Dec 29 2011 00:00:00 GMT-1000 (Apia Daylight Time)",
    DateString: "Thu Dec 29 2011",
    TimeString: "00:00:00 GMT-1000 (Apia Daylight Time)",
    UTCString: "Thu, 29 Dec 2011 10:00:00 GMT",
    ISOString: "2011-12-29T10:00:00.000Z",
    LocaleString: "Thu, 12/29/2011, 12:00:00 AM GMT-10",
    LocaleDateString: "Thu, 12/29/2011",
    LocaleTimeString: "12:00:00 AM GMT-10",
  });
}

{
  // +13:00 (daylight savings)
  let local = new DateTime.Local(2011, Month.December, 31, DayOfWeek.Saturday, 0, 0, 0);
  let utc = new DateTime.UTC(2011, Month.December, 30, DayOfWeek.Friday, 10, 0, 0);

  assertDate(local, utc, TimeZone(+14), {
    String: "Sat Dec 31 2011 00:00:00 GMT+1400 (Apia Daylight Time)",
    DateString: "Sat Dec 31 2011",
    TimeString: "00:00:00 GMT+1400 (Apia Daylight Time)",
    UTCString: "Fri, 30 Dec 2011 10:00:00 GMT",
    ISOString: "2011-12-30T10:00:00.000Z",
    LocaleString: "Sat, 12/31/2011, 12:00:00 AM GMT+14",
    LocaleDateString: "Sat, 12/31/2011",
    LocaleTimeString: "12:00:00 AM GMT+14",
  });
}

{
  // +13:00 (standard time)
  let local = new DateTime.Local(2012, Month.April, 2, DayOfWeek.Monday, 0, 0, 0);
  let utc = new DateTime.UTC(2012, Month.April, 1, DayOfWeek.Sunday, 11, 0, 0);

  assertDate(local, utc, TimeZone(+13), {
    String: "Mon Apr 02 2012 00:00:00 GMT+1300 (Apia Standard Time)",
    DateString: "Mon Apr 02 2012",
    TimeString: "00:00:00 GMT+1300 (Apia Standard Time)",
    UTCString: "Sun, 01 Apr 2012 11:00:00 GMT",
    ISOString: "2012-04-01T11:00:00.000Z",
    LocaleString: "Mon, 04/02/2012, 12:00:00 AM GMT+13",
    LocaleDateString: "Mon, 04/02/2012",
    LocaleTimeString: "12:00:00 AM GMT+13",
  });
}
});


// File: Date/Pacific_Chatham.js
// Pacific/Chatham time zone offset is 12:45.

inTimeZone("Pacific/Chatham", () => {
{
  // +12:45 (standard time)
  let local = new DateTime.Local(2010, Month.August, 1, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.July, 31, DayOfWeek.Saturday, 11, 15, 0);

  assertDate(local, utc, TimeZone(+12,45), {
    String: "Sun Aug 01 2010 00:00:00 GMT+1245 (Chatham Standard Time)",
    DateString: "Sun Aug 01 2010",
    TimeString: "00:00:00 GMT+1245 (Chatham Standard Time)",
    UTCString: "Sat, 31 Jul 2010 11:15:00 GMT",
    ISOString: "2010-07-31T11:15:00.000Z",
    LocaleString: "Sun, 08/01/2010, 12:00:00 AM GMT+12:45",
    LocaleDateString: "Sun, 08/01/2010",
    LocaleTimeString: "12:00:00 AM GMT+12:45",
  });
}

{
  // +12:45 (daylight savings)
  let local = new DateTime.Local(2010, Month.January, 3, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.January, 2, DayOfWeek.Saturday, 10, 15, 0);

  assertDate(local, utc, TimeZone(+13,45), {
    String: "Sun Jan 03 2010 00:00:00 GMT+1345 (Chatham Daylight Time)",
    DateString: "Sun Jan 03 2010",
    TimeString: "00:00:00 GMT+1345 (Chatham Daylight Time)",
    UTCString: "Sat, 02 Jan 2010 10:15:00 GMT",
    ISOString: "2010-01-02T10:15:00.000Z",
    LocaleString: "Sun, 01/03/2010, 12:00:00 AM GMT+13:45",
    LocaleDateString: "Sun, 01/03/2010",
    LocaleTimeString: "12:00:00 AM GMT+13:45",
  });
}
});


// File: Date/Pacific_Kiritimati.js
// Pacific/Kiritimati time zone offset is +14:00.

inTimeZone("Pacific/Kiritimati", () => {
{
  // +14:00 (standard time)
  let local = new DateTime.Local(2010, Month.August, 1, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(2010, Month.July, 31, DayOfWeek.Saturday, 10, 0, 0);

  assertDate(local, utc, TimeZone(+14), {
    String: "Sun Aug 01 2010 00:00:00 GMT+1400 (Line Islands Time)",
    DateString: "Sun Aug 01 2010",
    TimeString: "00:00:00 GMT+1400 (Line Islands Time)",
    UTCString: "Sat, 31 Jul 2010 10:00:00 GMT",
    ISOString: "2010-07-31T10:00:00.000Z",
    LocaleString: "Sun, 08/01/2010, 12:00:00 AM GMT+14",
    LocaleDateString: "Sun, 08/01/2010",
    LocaleTimeString: "12:00:00 AM GMT+14",
  });
}

// Pacific/Kiritimati time zone offset was -10:40 until Oct. 1979.

{
  // -10:40 (standard time)
  let local = new DateTime.Local(1975, Month.January, 1, DayOfWeek.Wednesday, 0, 0, 0);
  let utc = new DateTime.UTC(1975, Month.January, 1, DayOfWeek.Wednesday, 10, 40, 0);

  assertDate(local, utc, TimeZone(-10,40), {
    String: "Wed Jan 01 1975 00:00:00 GMT-1040 (Line Islands Time)",
    DateString: "Wed Jan 01 1975",
    TimeString: "00:00:00 GMT-1040 (Line Islands Time)",
    UTCString: "Wed, 01 Jan 1975 10:40:00 GMT",
    ISOString: "1975-01-01T10:40:00.000Z",
    LocaleString: "Wed, 01/01/1975, 12:00:00 AM GMT-10:40",
    LocaleDateString: "Wed, 01/01/1975",
    LocaleTimeString: "12:00:00 AM GMT-10:40",
  });
}
});


// File: Date/Pacifi_Niue.js
// Pacific/Niue time zone offset was -11:20 from 1952 through 1964.

inTimeZone("Pacific/Niue", () => {
{
  // -11:20 (standard time)
  let local = new DateTime.Local(1956, Month.January, 1, DayOfWeek.Sunday, 0, 0, 0);
  let utc = new DateTime.UTC(1956, Month.January, 1, DayOfWeek.Sunday, 11, 20, 0);

  assertDate(local, utc, TimeZone(-11,20), {
    String: "Sun Jan 01 1956 00:00:00 GMT-1120 (Niue Time)",
    DateString: "Sun Jan 01 1956",
    TimeString: "00:00:00 GMT-1120 (Niue Time)",
    UTCString: "Sun, 01 Jan 1956 11:20:00 GMT",
    ISOString: "1956-01-01T11:20:00.000Z",
    LocaleString: "Sun, 01/01/1956, 12:00:00 AM GMT-11:20",
    LocaleDateString: "Sun, 01/01/1956",
    LocaleTimeString: "12:00:00 AM GMT-11:20",
  });
}
});


if (typeof reportCompare === "function")
    reportCompare(true, true);
