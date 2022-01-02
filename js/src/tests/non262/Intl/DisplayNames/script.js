// |reftest| skip-if(!this.hasOwnProperty('Intl'))

const tests = {
  "en": {
    long: {
      "Latn": "Latin",
      "Hant": "Traditional Han",
      "Hans": "Simplified Han",
      "Cans": "Unified Canadian Aboriginal Syllabics",
    },
    short: {
      "Hant": "Traditional",
      "Hans": "Simplified",
      "Cans": "UCAS",
    },
    narrow: {},
  },
  "de": {
    long: {
      "Latn": "Lateinisch",
      "Hant": "Traditionelles Chinesisch",
      "Hans": "Vereinfachtes Chinesisch",
    },
    short: {
      "Hant": "Traditionell",
      "Hans": "Vereinfacht",
    },
    narrow: {},
  },
  "fr": {
    long: {
      "Latn": "latin",
      "Hant": "sinogrammes traditionnels",
      "Hans": "sinogrammes simplifiés",
    },
    short: {
      "Hant": "traditionnel",
      "Hans": "simplifié",
    },
    narrow: {},
  },
  "zh": {
    long: {
      "Latn": "拉丁文",
      "Hant": "繁体中文",
      "Hans": "简体中文",
    },
    short: {
      "Hant": "繁体",
      "Hans": "简体",
    },
    narrow: {},
  },
  "ar": {
    long: {
      "Latn": "اللاتينية",
      "Arab": "العربية",
      "Hant": "الهان التقليدية",
      "Hans": "الهان المبسطة",
    },
    short: {
      "Hant": "التقليدية",
      "Hans": "المبسطة",
    },
    narrow: {},
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "script", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "script");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [script, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(script), expected);

      // Also works with objects.
      assertEq(dn.of(Object(script)), expected);
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "script"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);

  // ToString(null) = "null", which passes `unicode_script_subtag`.
  dn.of(null); // no error

  // Throws an error if |code| can't be parsed as a `unicode_script_subtag` production.
  assertThrowsInstanceOf(() => dn.of("latn-"), RangeError);
  assertThrowsInstanceOf(() => dn.of("en-latn"), RangeError);
}

// Test fallback behaviour.
{
  let dn1 = new Intl.DisplayNames("en", {type: "script"});
  let dn2 = new Intl.DisplayNames("en", {type: "script", fallback: "code"});
  let dn3 = new Intl.DisplayNames("en", {type: "script", fallback: "none"});

  assertEq(dn1.resolvedOptions().fallback, "code");
  assertEq(dn2.resolvedOptions().fallback, "code");
  assertEq(dn3.resolvedOptions().fallback, "none");

  // "Aaaa" is not a registered script code.
  assertEq(dn1.of("Aaaa"), "Aaaa");
  assertEq(dn2.of("Aaaa"), "Aaaa");
  assertEq(dn3.of("Aaaa"), undefined);

  // The returned fallback is in canonical case.
  assertEq(dn1.of("aaaa"), "Aaaa");
  assertEq(dn2.of("aaaa"), "Aaaa");
  assertEq(dn3.of("aaaa"), undefined);
}

// Test when case isn't canonical.
{
  let dn = new Intl.DisplayNames("en", {type: "script", fallback: "none"});

  assertEq(dn.of("LATN"), "Latin");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
