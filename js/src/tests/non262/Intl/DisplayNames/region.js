// |reftest| skip-if(!this.hasOwnProperty('Intl'))

const tests = {
  "en": {
    long: {
      "DE": "Germany",
      "GB": "United Kingdom",
      "US": "United States",
      "FR": "France",
    },
    short: {
      "GB": "UK",
      "US": "US",
    },
    narrow: {},
  },
  "de": {
    long: {
      "DE": "Deutschland",
      "GB": "Vereinigtes Königreich",
      "US": "Vereinigte Staaten",
      "FR": "Frankreich",
    },
    short: {
      "GB": "UK",
      "US": "USA",
    },
    narrow: {},
  },
  "fr": {
    long: {
      "DE": "Allemagne",
      "GB": "Royaume-Uni",
      "US": "États-Unis",
      "FR": "France",
    },
    short: {
      "GB": "R.-U.",
      "US": "É.-U.",
    },
    narrow: {},
  },
  "zh": {
    long: {
      "CN": "中国",
      "HK": "中国香港特别行政区",
    },
    short: {
      "HK": "香港"
    },
    narrow: {},
  },
  "ar": {
    long: {
      "SA": "المملكة العربية السعودية",
      "MO": "منطقة ماكاو الإدارية الخاصة",
    },
    short: {
      "MO": "مكاو",
    },
    narrow: {},
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "region", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "region");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [region, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(region), expected);

      // Also works with objects.
      assertEq(dn.of(Object(region)), expected);
    }
  }
}

{
  let dn = new Intl.DisplayNames("en", {type: "region"});

  // Performs ToString on the input and then validates the stringified result.
  assertThrowsInstanceOf(() => dn.of(), RangeError);
  assertThrowsInstanceOf(() => dn.of(null), RangeError);
  assertThrowsInstanceOf(() => dn.of(Symbol()), TypeError);
  assertThrowsInstanceOf(() => dn.of(0), RangeError);

  // Throws an error if |code| can't be parsed as a `unicode_region_subtag` production.
  assertThrowsInstanceOf(() => dn.of("CA-"), RangeError);
  assertThrowsInstanceOf(() => dn.of("en-CA"), RangeError);
}

// Test fallback behaviour.
{
  let dn1 = new Intl.DisplayNames("en", {type: "region"});
  let dn2 = new Intl.DisplayNames("en", {type: "region", fallback: "code"});
  let dn3 = new Intl.DisplayNames("en", {type: "region", fallback: "none"});

  assertEq(dn1.resolvedOptions().fallback, "code");
  assertEq(dn2.resolvedOptions().fallback, "code");
  assertEq(dn3.resolvedOptions().fallback, "none");

  // "AA" is not a registered region code.
  assertEq(dn1.of("AA"), "AA");
  assertEq(dn2.of("AA"), "AA");
  assertEq(dn3.of("AA"), undefined);

  // The returned fallback is in canonical case.
  assertEq(dn1.of("aa"), "AA");
  assertEq(dn2.of("aa"), "AA");
  assertEq(dn3.of("aa"), undefined);

  // "998" is canonicalised to "XZ", but "XZ" has no localised names.
  assertEq(new Intl.Locale("und-998").region, "XZ");

  // Ensure we return the input and not the canonicalised input.
  assertEq(dn1.of("998"), "998");
  assertEq(dn2.of("998"), "998");
  assertEq(dn3.of("998"), undefined);

  // "XZ" should be consistent with "998".
  assertEq(dn1.of("XZ"), "XZ");
  assertEq(dn2.of("XZ"), "XZ");
  assertEq(dn3.of("XZ"), undefined);
}

// Ensure language tag canonicalisation is performed.
{
  let dn = new Intl.DisplayNames("en", {type: "region", fallback: "none"});

  assertEq(dn.of("RU"), "Russia");

  // ICU's canonicalisation supports "SU" -> "RU".
  assertEq(Intl.getCanonicalLocales("ru-SU")[0], "ru-RU");
  assertEq(dn.of("SU"), "Russia");

  // ICU's canonicalisation doesn't support "172" -> "RU".
  assertEq(Intl.getCanonicalLocales("ru-172")[0], "ru-RU");
  assertEq(dn.of("172"), "Russia");
}

// Test when case isn't canonical.
{
  let dn = new Intl.DisplayNames("en", {type: "region", fallback: "none"});

  assertEq(dn.of("IT"), "Italy");
  assertEq(dn.of("it"), "Italy");
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
