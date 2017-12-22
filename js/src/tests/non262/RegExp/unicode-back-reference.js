var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- back reference should not match lead surrogate that has corresponding trail surrogate.";

print(BUGNUMBER + ": " + summary);

// The last character of back reference is not a surrogate.
assertEqArray(/foo(.+)bar\1/u.exec("fooAbarA\uDC00"),
              ["fooAbarA", "A"]);
assertEqArray(/foo(.+)bar\1/u.exec("fooAbarA\uD834"),
              ["fooAbarA", "A"]);
assertEqArray(/foo(.+)bar\1/u.exec("fooAbarAA"),
              ["fooAbarA", "A"]);
assertEqArray(/foo(.+)bar\1/u.exec("fooAbarA"),
              ["fooAbarA", "A"]);

// The last character of back reference is a lead surrogate.
assertEq(/foo(.+)bar\1/u.exec("foo\uD834bar\uD834\uDC00"), null);
assertEqArray(/foo(.+)bar\1/u.exec("foo\uD834bar\uD834\uD834"),
              ["foo\uD834bar\uD834", "\uD834"]);
assertEqArray(/foo(.+)bar\1/u.exec("foo\uD834bar\uD834A"),
              ["foo\uD834bar\uD834", "\uD834"]);
assertEqArray(/foo(.+)bar\1/u.exec("foo\uD834bar\uD834"),
              ["foo\uD834bar\uD834", "\uD834"]);

// The last character of back reference is a trail surrogate.
assertEqArray(/foo(.+)bar\1/u.exec("foo\uDC00bar\uDC00\uDC00"),
              ["foo\uDC00bar\uDC00", "\uDC00"]);
assertEqArray(/foo(.+)bar\1/u.exec("foo\uDC00bar\uDC00\uD834"),
              ["foo\uDC00bar\uDC00", "\uDC00"]);
assertEqArray(/foo(.+)bar\1/u.exec("foo\uDC00bar\uDC00A"),
              ["foo\uDC00bar\uDC00", "\uDC00"]);
assertEqArray(/foo(.+)bar\1/u.exec("foo\uDC00bar\uDC00"),
              ["foo\uDC00bar\uDC00", "\uDC00"]);

// Pattern should not match to surrogate pair partially.
assertEq(/^(.+)\1$/u.exec("\uDC00foobar\uD834\uDC00foobar\uD834"), null);

if (typeof reportCompare === "function")
    reportCompare(true, true);
