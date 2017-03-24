var {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/Services.jsm");

function run_test()
{
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
    var collator = Cc["@mozilla.org/intl/collation-factory;1"].
      createInstance(Ci.nsICollationFactory).
      CreateCollationForLocale(locale);
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
