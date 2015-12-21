var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- ignoreCase flag with non-ascii to ascii map.";

print(BUGNUMBER + ": " + summary);

// LATIN CAPITAL LETTER Y WITH DIAERESIS
assertEqArray(/\u0178/iu.exec("\u00FF"),
              ["\u00FF"]);
assertEqArray(/\u00FF/iu.exec("\u0178"),
              ["\u0178"]);

// LATIN SMALL LETTER LONG S
assertEqArray(/\u017F/iu.exec("S"),
              ["S"]);
assertEqArray(/\u017F/iu.exec("s"),
              ["s"]);
assertEqArray(/S/iu.exec("\u017F"),
              ["\u017F"]);
assertEqArray(/s/iu.exec("\u017F"),
              ["\u017F"]);

// LATIN CAPITAL LETTER SHARP S
assertEqArray(/\u1E9E/iu.exec("\u00DF"),
              ["\u00DF"]);
assertEqArray(/\u00DF/iu.exec("\u1E9E"),
              ["\u1E9E"]);

// KELVIN SIGN
assertEqArray(/\u212A/iu.exec("K"),
              ["K"]);
assertEqArray(/\u212A/iu.exec("k"),
              ["k"]);
assertEqArray(/K/iu.exec("\u212A"),
              ["\u212A"]);
assertEqArray(/k/iu.exec("\u212A"),
              ["\u212A"]);

// ANGSTROM SIGN
assertEqArray(/\u212B/iu.exec("\u00E5"),
              ["\u00E5"]);
assertEqArray(/\u00E5/iu.exec("\u212B"),
              ["\u212B"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
