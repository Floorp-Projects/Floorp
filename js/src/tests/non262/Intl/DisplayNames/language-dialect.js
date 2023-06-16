// |reftest| skip-if(!this.hasOwnProperty('Intl'))

const tests = {
  "en": {
    long: {
      "de": "German",
      "de-AT": "Austrian German",
      "de-1996": "German (German orthography of 1996)",
      "en": "English",
      "en-Hant-GB": "British English (Traditional)",
      "en-Hans-US": "American English (Simplified)",
      "fr": "French",
      "nl-BE": "Flemish",
      "cr-Cans": "Cree (Unified Canadian Aboriginal Syllabics)",
    },
    short: {
      "en-Hant-GB": "UK English (Traditional)",
      "en-Hans-US": "US English (Simplified)",
      "cr-Cans": "Cree (UCAS)",
    },
    narrow: {},
  },
  "de": {
    long: {
      "de": "Deutsch",
      "de-AT": "Österreichisches Deutsch",
      "de-1996": "Deutsch (Neue deutsche Rechtschreibung)",
      "en": "Englisch",
      "en-Hant-GB": "Englisch (Traditionell, Vereinigtes Königreich)",
      "en-Hans-US": "Englisch (Vereinfacht, Vereinigte Staaten)",
      "fr": "Französisch",
      "nl-BE": "Flämisch",
    },
    short: {
      "en-Hant-GB": "Englisch (GB) (Traditionell)",
      "en-Hans-US": "Englisch (USA) (Vereinfacht)",
    },
    narrow: {},
  },
  "fr": {
    long: {
      "de": "allemand",
      "de-AT": "allemand autrichien",
      "de-1996": "allemand (orthographe allemande de 1996)",
      "en": "anglais",
      "en-Hant-GB": "anglais britannique (traditionnel)",
      "en-Hans-US": "anglais américain (simplifié)",
      "fr": "français",
      "nl-BE": "flamand",
    },
    short: {
      "en-Hant-GB": "anglais britannique (traditionnel)",
      "en-Hans-US": "anglais américain (simplifié)",
    },
    narrow: {},
  },
  "zh": {
    long: {
      "zh": "中文",
      "zh-Hant": "繁体中文",
      "zh-Hant-CN": "繁体中文（中国）",
      "zh-Hans-HK": "简体中文（中国香港特别行政区）",
    },
    short: {
      "zh-Hans-HK": "简体中文（香港）"
    },
    narrow: {},
  },
  "ar": {
    long: {
      "ar": "العربية",
      "ar-SA": "العربية (المملكة العربية السعودية)",
      "zh-MO": "الصينية (منطقة ماكاو الإدارية الخاصة)",
    },
    short: {
      "zh-MO": "الصينية (مكاو)",
    },
    narrow: {},
  },
};

for (let [locale, localeTests] of Object.entries(tests)) {
  for (let [style, styleTests] of Object.entries(localeTests)) {
    let dn = new Intl.DisplayNames(locale, {type: "language", languageDisplay: "dialect", style});

    let resolved = dn.resolvedOptions();
    assertEq(resolved.locale, locale);
    assertEq(resolved.style, style);
    assertEq(resolved.type, "language");
    assertEq(resolved.languageDisplay, "dialect");
    assertEq(resolved.fallback, "code");

    let inheritedTests = {...localeTests.long, ...localeTests.short, ...localeTests.narrow};
    for (let [language, expected] of Object.entries({...inheritedTests, ...styleTests})) {
      assertEq(dn.of(language), expected);

      // Also works with objects.
      assertEq(dn.of(Object(language)), expected);
    }
  }
}

if (typeof reportCompare === "function")
  reportCompare(true, true);
