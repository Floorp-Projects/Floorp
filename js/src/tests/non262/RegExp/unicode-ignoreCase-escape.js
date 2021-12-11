var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- ignoreCase flag with character class escape.";

// \W doesn't match S or K from the change in
// https://github.com/tc39/ecma262/pull/525
// (bug 1281739)

print(BUGNUMBER + ": " + summary);

// LATIN SMALL LETTER LONG S

assertEqArray(/\w/iu.exec("S"),
              ["S"]);
assertEqArray(/\w/iu.exec("s"),
              ["s"]);
assertEqArray(/\w/iu.exec("\u017F"),
              ["\u017F"]);

assertEqArray(/[^\W]/iu.exec("S"),
              ["S"]);
assertEqArray(/[^\W]/iu.exec("s"),
              ["s"]);
assertEqArray(/[^\W]/iu.exec("\u017F"),
              ["\u017F"]);

assertEq(/\W/iu.exec("S"),
         null);
assertEq(/\W/iu.exec("s"),
         null);
assertEq(/\W/iu.exec("\u017F"),
         null);

assertEq(/[^\w]/iu.exec("S"),
         null);
assertEq(/[^\w]/iu.exec("s"),
         null);
assertEq(/[^\w]/iu.exec("\u017F"),
         null);

// KELVIN SIGN

assertEqArray(/\w/iu.exec("k"),
              ["k"]);
assertEqArray(/\w/iu.exec("k"),
              ["k"]);
assertEqArray(/\w/iu.exec("\u212A"),
              ["\u212A"]);

assertEqArray(/[^\W]/iu.exec("k"),
              ["k"]);
assertEqArray(/[^\W]/iu.exec("k"),
              ["k"]);
assertEqArray(/[^\W]/iu.exec("\u212A"),
              ["\u212A"]);

assertEq(/\W/iu.exec("k"),
         null);
assertEq(/\W/iu.exec("k"),
         null);
assertEq(/\W/iu.exec("\u212A"),
         null);

assertEq(/[^\w]/iu.exec("k"),
         null);
assertEq(/[^\w]/iu.exec("k"),
         null);
assertEq(/[^\w]/iu.exec("\u212A"),
         null);

if (typeof reportCompare === "function")
    reportCompare(true, true);
