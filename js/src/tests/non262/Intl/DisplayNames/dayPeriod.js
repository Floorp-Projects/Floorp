// |reftest| skip-if(!this.hasOwnProperty('Intl')||!this.hasOwnProperty('addIntlExtras'))

addMozIntlDisplayNames(this);

const tests = {
  "en": {
    long: {
      "am": "AM",
      "pm": "PM",
    },
    short: {},
    narrow: {},
  },
  "de": {
    long: {
      "am": "AM",
      "pm": "PM",
    },
    short: {},
    narrow: {},
  },
  "fr": {
    long: {
      "am": "AM",
      "pm": "PM",
    },
    short: {},
    narrow: {},
  },
  "zh": {
    long: {
      "am": "上午",
      "pm": "下午",
    },
    short: {},
    narrow: {},
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  let defaultCalendar = new Intl.DateTimeFormat(locale).resolvedOptions().calendar;

  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "dayPeriod", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.calendar, defaultCalendar);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "dayPeriod");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [dayPeriod, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(dayPeriod), expected);

      // Also works with objects.
      assertEq(dn.of(Object(dayPeriod)), expected);
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "dayPeriod"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(null), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);
  assertThrowsInstanceOf(() => dn.of(1), RangeError);

  // Throws an error if not one of ["am", "pm"].
  assertThrowsInstanceOf(() => dn.of(""), RangeError);
  assertThrowsInstanceOf(() => dn.of("AM"), RangeError);
  assertThrowsInstanceOf(() => dn.of("PM"), RangeError);
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
