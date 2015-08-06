var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- negated CharacterClass.";

print(BUGNUMBER + ": " + summary);

// ==== BMP ====

assertEqArray(/[^A]/u.exec("ABC"),
              ["B"]);
assertEqArray(/[^A]/u.exec("A\u{1F438}C"),
              ["\u{1F438}"]);
assertEqArray(/[^A]/u.exec("A\uD83DC"),
              ["\uD83D"]);
assertEqArray(/[^A]/u.exec("A\uDC38C"),
              ["\uDC38"]);

assertEqArray(/[^\uE000]/u.exec("\uE000\uE001"),
              ["\uE001"]);
assertEqArray(/[^\uE000]/u.exec("\uE000\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/[^\uE000]/u.exec("\uE000\uD83D"),
              ["\uD83D"]);
assertEqArray(/[^\uE000]/u.exec("\uE000\uDC38"),
              ["\uDC38"]);

// ==== non-BMP ====

assertEqArray(/[^\u{1F438}]/u.exec("\u{1F438}A"),
              ["A"]);
assertEqArray(/[^\u{1F438}]/u.exec("\u{1F438}\u{1F439}"),
              ["\u{1F439}"]);
assertEqArray(/[^\u{1F438}]/u.exec("\u{1F438}\uD83D"),
              ["\uD83D"]);
assertEqArray(/[^\u{1F438}]/u.exec("\u{1F438}\uDC38"),
              ["\uDC38"]);

// ==== lead-only ====

assertEqArray(/[^\uD83D]/u.exec("\u{1F438}A"),
              ["\u{1F438}"]);
assertEqArray(/[^\uD83D]/u.exec("\uD83D\uDBFF"),
              ["\uDBFF"]);
assertEqArray(/[^\uD83D]/u.exec("\uD83D\uDC00"),
              ["\uD83D\uDC00"]);
assertEqArray(/[^\uD83D]/u.exec("\uD83D\uDFFF"),
              ["\uD83D\uDFFF"]);
assertEqArray(/[^\uD83D]/u.exec("\uD83D\uE000"),
              ["\uE000"]);

// ==== trail-only ====

assertEqArray(/[^\uDC38]/u.exec("\u{1F438}A"),
              ["\u{1F438}"]);
assertEqArray(/[^\uDC38]/u.exec("\uD7FF\uDC38"),
              ["\uD7FF"]);
assertEqArray(/[^\uDC38]/u.exec("\uD800\uDC38"),
              ["\uD800\uDC38"]);
assertEqArray(/[^\uDC38]/u.exec("\uDBFF\uDC38"),
              ["\uDBFF\uDC38"]);
assertEqArray(/[^\uDC38]/u.exec("\uDC00\uDC38"),
              ["\uDC00"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
