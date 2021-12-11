var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- everything Atom.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assertEqArray(/./u.exec("ABC"),
              ["A"]);
assertEqArray(/./u.exec("\u{1F438}BC"),
              ["\u{1F438}"]);

assertEqArray(/./u.exec("\uD83D\uDBFF"),
              ["\uD83D"]);
assertEqArray(/./u.exec("\uD83D\uDC00"),
              ["\uD83D\uDC00"]);
assertEqArray(/./u.exec("\uD83D\uDFFF"),
              ["\uD83D\uDFFF"]);
assertEqArray(/./u.exec("\uD83D\uE000"),
              ["\uD83D"]);
assertEqArray(/./u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/./u.exec("\uD83DA"),
              ["\uD83D"]);

assertEqArray(/./u.exec("\uD7FF\uDC38"),
              ["\uD7FF"]);
assertEqArray(/./u.exec("\uD800\uDC38"),
              ["\uD800\uDC38"]);
assertEqArray(/./u.exec("\uDBFF\uDC38"),
              ["\uDBFF\uDC38"]);
assertEqArray(/./u.exec("\uDC00\uDC38"),
              ["\uDC00"]);
assertEqArray(/./u.exec("\uDC38"),
              ["\uDC38"]);
assertEqArray(/./u.exec("A\uDC38"),
              ["A"]);

assertEqArray(/.A/u.exec("\uD7FF\uDC38A"),
              ["\uDC38A"]);
assertEqArray(/.A/u.exec("\uD800\uDC38A"),
              ["\uD800\uDC38A"]);
assertEqArray(/.A/u.exec("\uDBFF\uDC38A"),
              ["\uDBFF\uDC38A"]);
assertEqArray(/.A/u.exec("\uDC00\uDC38A"),
              ["\uDC38A"]);

// ==== leading multiple ====

assertEqArray(/.*A/u.exec("\u{1F438}\u{1F438}\u{1F438}A"),
              ["\u{1F438}\u{1F438}\u{1F438}A"]);

// ==== trailing multiple ====

assertEqArray(/A.*/u.exec("A\u{1F438}\u{1F438}\u{1F438}"),
              ["A\u{1F438}\u{1F438}\u{1F438}"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
