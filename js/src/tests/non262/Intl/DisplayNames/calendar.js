// |reftest| skip-if(!this.hasOwnProperty('Intl'))

const tests = {
  "en": {
    long: {
      "gregory": "Gregorian Calendar",
      "iso8601": "ISO-8601 Calendar",
      "japanese": "Japanese Calendar",
      "islamic-civil": "Islamic Calendar (tabular, civil epoch)",
      "islamicc": "Islamic Calendar (tabular, civil epoch)",
      "ethioaa": "Ethiopic Amete Alem Calendar",
      "ethiopic-amete-alem": "Ethiopic Amete Alem Calendar",
    },
    short: {},
    narrow: {},
  },
  "de": {
    long: {
      "gregory": "Gregorianischer Kalender",
      "iso8601": "ISO-8601-Kalender",
      "japanese": "Japanischer Kalender",
      "islamic-civil": "Bürgerlicher islamischer Kalender (tabellarisch)",
      "islamicc": "Bürgerlicher islamischer Kalender (tabellarisch)",
      "ethioaa": "Äthiopischer Amätä-Aläm-Kalender",
      "ethiopic-amete-alem": "Äthiopischer Amätä-Aläm-Kalender",
    },
    short: {},
    narrow: {},
  },
  "fr": {
    long: {
      "gregory": "calendrier grégorien",
      "iso8601": "calendrier ISO 8601",
      "japanese": "calendrier japonais",
      "islamic-civil": "calendrier musulman (tabulaire, époque civile)",
      "islamicc": "calendrier musulman (tabulaire, époque civile)",
      "ethioaa": "calendrier éthiopien Amete Alem",
      "ethiopic-amete-alem": "calendrier éthiopien Amete Alem",
    },
    short: {},
    narrow: {},
  },
  "zh": {
    long: {
      "gregory": "公历",
      "iso8601": "国际标准历法",
      "japanese": "和历",
      "islamic-civil": "伊斯兰希吉来日历",
      "islamicc": "伊斯兰希吉来日历",
      "ethioaa": "埃塞俄比亚阿米特阿莱姆日历",
      "ethiopic-amete-alem": "埃塞俄比亚阿米特阿莱姆日历",
    },
    short: {},
    narrow: {},
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "calendar", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "calendar");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [calendar, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(calendar), expected);

      // Also works with objects.
      assertEq(dn.of(Object(calendar)), expected);
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "calendar"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(undefined), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);

  // Throws an error if |code| isn't a well-formed calendar type.
  assertThrowsInstanceOf(() => dn.of("gregorian"), RangeError);
  assertThrowsInstanceOf(() => dn.of("grëgory"), RangeError);
  assertThrowsInstanceOf(() => dn.of("grēgory"), RangeError);
}

// Test fallback behaviour.
{
  let dn1 = new Intl.DisplayNames("en", {type: "calendar"});
  let dn2 = new Intl.DisplayNames("en", {type: "calendar", fallback: "code"});
  let dn3 = new Intl.DisplayNames("en", {type: "calendar", fallback: "none"});

  assertEq(dn1.resolvedOptions().fallback, "code");
  assertEq(dn2.resolvedOptions().fallback, "code");
  assertEq(dn3.resolvedOptions().fallback, "none");

  // "invalid" isn't a known calendar type.
  assertEq(dn1.of("invalid"), "invalid");
  assertEq(dn2.of("invalid"), "invalid");
  assertEq(dn3.of("invalid"), undefined);

  // The returned fallback is in canonical case.
  assertEq(dn1.of("INVALID"), "invalid");
  assertEq(dn2.of("INVALID"), "invalid");
  assertEq(dn3.of("INVALID"), undefined);
}

// Test when case isn't canonical.
{
  let dn = new Intl.DisplayNames("en", {type: "calendar", fallback: "none"});

  assertEq(dn.of("gregory"), "Gregorian Calendar");
  assertEq(dn.of("GREGORY"), "Gregorian Calendar");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
