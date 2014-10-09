const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

function check_sort() {
  var input = [
    "Argentina",
    "Oerlikon",
    "Offenbach",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "la France",
    "¡viva España!",
    "Österreich",
    "中国",
    "日本",
    "한국",
  ];

  function test(locale, expected) {
    var localeSvc = Cc["@mozilla.org/intl/nslocaleservice;1"].
      getService(Ci.nsILocaleService);
    var collator = Cc["@mozilla.org/intl/collation-factory;1"].
      createInstance(Ci.nsICollationFactory).
      CreateCollation(localeSvc.newLocale(locale));
    var strength = Ci.nsICollation.kCollationStrengthDefault;
    var actual = input.sort((x, y) => collator.compareString(strength, x,y));
    deepEqual(actual, expected, locale);
  }

  // Locale en-US; default options.
  test("en-US", [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Österreich",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "한국",
    "中国",
    "日本",
  ]);

  // Locale sv-SE; default options.
  // Swedish treats "Ö" as a separate character, which sorts after "Z".
  test("sv-SE", [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "Österreich",
    "한국",
    "中国",
    "日本",
  ]);

  // Locale de-DE; default options.
  // In German standard sorting, umlauted characters are treated as variants
  // of their base characters: ä ≅ a, ö ≅ o, ü ≅ u.
  test("de-DE", [
    "¡viva España!",
    "Argentina",
    "la France",
    "Oerlikon",
    "Offenbach",
    "Österreich",
    "Sverige",
    "Vaticano",
    "Zimbabwe",
    "한국",
    "中国",
    "日本",
  ]);
}

function test_default() {
  Services.prefs.clearUserPref("intl.collation.mac.use_icu");
  check_sort();
}

function test_ICU() {
  Services.prefs.setBoolPref("intl.collation.mac.use_icu", true);
  check_sort();
}

function test_CoreServices() {
  Services.prefs.setBoolPref("intl.collation.mac.use_icu", false);
  check_sort();
}

function run_test()
{
  test_default();
  test_ICU();
  if (Services.sysinfo.getProperty("arch") == "x86") {
    test_CoreServices();
  }

  Services.prefs.clearUserPref("intl.collation.mac.use_icu");
}
