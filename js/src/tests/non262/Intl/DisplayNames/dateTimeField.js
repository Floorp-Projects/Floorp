// |reftest| skip-if(!this.hasOwnProperty('Intl'))

const tests = {
  "en": {
    long: {
      "era": "era",
      "year": "year",
      "quarter": "quarter",
      "month": "month",
      "weekOfYear": "week",
      "weekday": "day of the week",
      "day": "day",
      "dayPeriod": "AM/PM",
      "hour": "hour",
      "minute": "minute",
      "second": "second",
      "timeZoneName": "time zone",
    },
    short: {
      "year": "yr.",
      "quarter": "qtr.",
      "month": "mo.",
      "weekOfYear": "wk.",
      "weekday": "day of wk.",
      "dayPeriod": "AM/PM",
      "hour": "hr.",
      "minute": "min.",
      "second": "sec.",
      "timeZoneName": "zone",
    },
    narrow: {
      "year": "yr",
      "quarter": "qtr",
      "month": "mo",
      "weekOfYear": "wk",
      "hour": "hr",
      "minute": "min",
      "second": "sec",
    },
  },
  "de": {
    long: {
      "era": "Epoche",
      "year": "Jahr",
      "quarter": "Quartal",
      "month": "Monat",
      "weekOfYear": "Woche",
      "weekday": "Wochentag",
      "day": "Tag",
      "dayPeriod": "Tageshälfte",
      "hour": "Stunde",
      "minute": "Minute",
      "second": "Sekunde",
      "timeZoneName": "Zeitzone",
    },
    short: {
      "era": "Epoche",
      "year": "Jahr",
      "quarter": "Quart.",
      "month": "Monat",
      "weekOfYear": "Woche",
      "weekday": "Wochentag",
      "day": "Tag",
      "dayPeriod": "Tageshälfte",
      "hour": "Std.",
      "minute": "Min.",
      "second": "Sek.",
      "timeZoneName": "Zeitzone",
    },
    narrow: {
      "era": "E",
      "year": "J",
      "quarter": "Q",
      "month": "M",
      "weekOfYear": "W",
      "weekday": "Wochent.",
      "dayPeriod": "Tagesh.",
      "timeZoneName": "Zeitz.",
    },
  },
  "fr": {
    long: {
      "era": "ère",
      "year": "année",
      "quarter": "trimestre",
      "month": "mois",
      "weekOfYear": "semaine",
      "weekday": "jour de la semaine",
      "day": "jour",
      "dayPeriod": "cadran",
      "hour": "heure",
      "minute": "minute",
      "second": "seconde",
      "timeZoneName": "fuseau horaire",
    },
    short: {
      "year": "an",
      "quarter": "trim.",
      "month": "m.",
      "weekOfYear": "sem.",
      "weekday": "j (sem.)",
      "day": "j",
      "hour": "h",
      "minute": "min",
      "second": "s",
    },
    narrow: {
      "year": "a",
    },
  },
  "zh": {
    long: {
      "era": "纪元",
      "year": "年",
      "quarter": "季度",
      "month": "月",
      "weekOfYear": "周",
      "weekday": "工作日",
      "day": "日",
      "dayPeriod": "上午/下午",
      "hour": "小时",
      "minute": "分钟",
      "second": "秒",
      "timeZoneName": "时区",
    },
    short: {
      "quarter": "季",
      "minute": "分",
    },
    narrow: {},
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "dateTimeField", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "dateTimeField");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [field, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(field), expected);

      // Also works with objects.
      assertEq(dn.of(Object(field)), expected);
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "dateTimeField"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(null), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);
  assertThrowsInstanceOf(() => dn.of(1), RangeError);

  // Throws an error if not one of ["era", "year", "quarter", "month", "weekOfYear", "weekday",
  // "day", "dayPeriod", "hour", "minute", "second", "timeZoneName"].
  assertThrowsInstanceOf(() => dn.of(""), RangeError);
  assertThrowsInstanceOf(() => dn.of("ERA"), RangeError);
  assertThrowsInstanceOf(() => dn.of("Era"), RangeError);
  assertThrowsInstanceOf(() => dn.of("era\0"), RangeError);
  assertThrowsInstanceOf(() => dn.of("dayperiod"), RangeError);
  assertThrowsInstanceOf(() => dn.of("day-period"), RangeError);
  assertThrowsInstanceOf(() => dn.of("timezoneName"), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
