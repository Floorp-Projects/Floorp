var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- ignoreCase flag with character class escape.";

print(BUGNUMBER + ": " + summary);

// LATIN SMALL LETTER LONG S

assertEqArray(/\w/iu.exec("S"),
              ["S"]);
assertEqArray(/\w/iu.exec("s"),
              ["s"]);
assertEqArray(/\w/iu.exec("\u017F"),
              ["\u017F"]);

assertEqArray(/\W/iu.exec("S"),
              ["S"]);
assertEqArray(/\W/iu.exec("s"),
              ["s"]);
assertEqArray(/\W/iu.exec("\u017F"),
              ["\u017F"]);

// KELVIN SIGN

assertEqArray(/\w/iu.exec("k"),
              ["k"]);
assertEqArray(/\w/iu.exec("k"),
              ["k"]);
assertEqArray(/\w/iu.exec("\u212A"),
              ["\u212A"]);

assertEqArray(/\W/iu.exec("k"),
              ["k"]);
assertEqArray(/\W/iu.exec("k"),
              ["k"]);
assertEqArray(/\W/iu.exec("\u212A"),
              ["\u212A"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
