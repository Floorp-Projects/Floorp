var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- ignoreCase flag with negated character class.";

print(BUGNUMBER + ": " + summary);

assertEq(/[^A]/iu.exec("A"),
         null);
assertEq(/[^a]/iu.exec("A"),
         null);
assertEq(/[^A]/iu.exec("a"),
         null);
assertEq(/[^a]/iu.exec("a"),
         null);

assertEqArray(/[^A]/iu.exec("b"),
              ["b"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
