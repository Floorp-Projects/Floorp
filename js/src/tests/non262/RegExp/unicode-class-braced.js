var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- braced pattern in RegExpUnicodeEscapeSequence in CharacterClass.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assertEqArray(/[\u{41}]/u.exec("ABC"),
              ["A"]);

assertEqArray(/[\u{1F438}]/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEq(/[\u{1F438}]/u.exec("\uD83D"),
         null);
assertEq(/[\u{1F438}]/u.exec("\uDC38"),
         null);

assertEqArray(/[\u{0}]/u.exec("\u{0}"),
              ["\u{0}"]);
assertEqArray(/[\u{10FFFF}]/u.exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);
assertEqArray(/[\u{10ffff}]/u.exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);

// leading 0
assertEqArray(/[\u{0000000000000000000000}]/u.exec("\u{0}"),
              ["\u{0}"]);
assertEqArray(/[\u{000000000000000010FFFF}]/u.exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);

// RegExp constructor
assertEqArray(new RegExp("[\\u{0}]", "u").exec("\u{0}"),
              ["\u{0}"]);
assertEqArray(new RegExp("[\\u{41}]", "u").exec("ABC"),
              ["A"]);
assertEqArray(new RegExp("[\\u{1F438}]", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(new RegExp("[\\u{10FFFF}]", "u").exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);

assertEqArray(new RegExp("[\\u{0000000000000000}]", "u").exec("\u{0}"),
              ["\u{0}"]);

assertEqArray(eval(`/[\\u{${"0".repeat(Math.pow(2, 24)) + "1234"}}]/u`).exec("\u{1234}"),
              ["\u{1234}"]);
assertEqArray(new RegExp(`[\\u{${"0".repeat(Math.pow(2, 24)) + "1234"}}]`, "u").exec("\u{1234}"),
              ["\u{1234}"]);

// ==== BMP + non-BMP ====

assertEqArray(/[A\u{1F438}]/u.exec("A\u{1F438}"),
              ["A"]);
assertEqArray(/[A\u{1F438}]/u.exec("\u{1F438}A"),
              ["\u{1F438}"]);

// lead-only target
assertEqArray(/[A\u{1F438}]/u.exec("\uD83DA"),
              ["A"]);
assertEq(/[A\u{1F438}]/u.exec("\uD83D"),
         null);

// +
assertEqArray(/[A\u{1F438}]+/u.exec("\u{1F438}A\u{1F438}A"),
              ["\u{1F438}A\u{1F438}A"]);

// trail surrogate + lead surrogate
assertEqArray(/[A\u{1F438}]+/u.exec("\uD83D\uDC38A\uDC38\uD83DA"),
              ["\uD83D\uDC38A"]);

// ==== non-BMP + non-BMP ====

assertEqArray(/[\u{1F418}\u{1F438}]/u.exec("\u{1F418}\u{1F438}"),
              ["\u{1F418}"]);

assertEqArray(/[\u{1F418}\u{1F438}]+/u.exec("\u{1F418}\u{1F438}"),
              ["\u{1F418}\u{1F438}"]);
assertEqArray(/[\u{1F418}\u{1F438}]+/u.exec("\u{1F418}\uDC38\uD83D"),
              ["\u{1F418}"]);
assertEqArray(/[\u{1F418}\u{1F438}]+/u.exec("\uDC18\uD83D\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/[\u{1F418}\u{1F438}]+/u.exec("\uDC18\u{1F438}\uD83D"),
              ["\u{1F438}"]);

// trail surrogate + lead surrogate
assertEq(/[\u{1F418}\u{1F438}]+/u.exec("\uDC18\uDC38\uD83D\uD83D"),
         null);

// ==== non-BMP + non-BMP range (from_lead == to_lead) ====

assertEqArray(/[\u{1F418}-\u{1F438}]/u.exec("\u{1F418}"),
              ["\u{1F418}"]);
assertEqArray(/[\u{1F418}-\u{1F438}]/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/[\u{1F418}-\u{1F438}]/u.exec("\u{1F427}"),
              ["\u{1F427}"]);

assertEq(/[\u{1F418}-\u{1F438}]/u.exec("\u{1F417}"),
         null);
assertEq(/[\u{1F418}-\u{1F438}]/u.exec("\u{1F439}"),
         null);

// ==== non-BMP + non-BMP range (from_lead + 1 == to_lead) ====

assertEqArray(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83C\uDD7C"),
              ["\uD83C\uDD7C"]);
assertEqArray(/[\u{1F17C}-\u{1F438}]/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83C\uDF99"),
              ["\uD83C\uDF99"]);
assertEqArray(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83D\uDC00"),
              ["\uD83D\uDC00"]);

assertEq(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83C\uDD7B"),
         null);
assertEq(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83C\uE000"),
         null);
assertEq(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83D\uDB99"),
         null);
assertEq(/[\u{1F17C}-\u{1F438}]/u.exec("\uD83D\uDC39"),
         null);

// ==== non-BMP + non-BMP range (from_lead + 2 == to_lead) ====

assertEqArray(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83C\uDD7C"),
              ["\uD83C\uDD7C"]);
assertEqArray(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83E\uDC29"),
              ["\uD83E\uDC29"]);

assertEqArray(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83C\uDF99"),
              ["\uD83C\uDF99"]);
assertEqArray(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83D\uDC00"),
              ["\uD83D\uDC00"]);
assertEqArray(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83D\uDF99"),
              ["\uD83D\uDF99"]);
assertEqArray(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83E\uDC00"),
              ["\uD83E\uDC00"]);

assertEq(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83C\uDD7B"),
         null);
assertEq(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83C\uE000"),
         null);
assertEq(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83D\uDB99"),
         null);
assertEq(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83D\uE000"),
         null);
assertEq(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83E\uDB99"),
         null);
assertEq(/[\u{1F17C}-\u{1F829}]/u.exec("\uD83E\uDC30"),
         null);

// ==== non-BMP + non-BMP range (other) ====

assertEqArray(/[\u{1D164}-\u{1F438}]/u.exec("\uD834\uDD64"),
              ["\uD834\uDD64"]);
assertEqArray(/[\u{1D164}-\u{1F438}]/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/[\u{1D164}-\u{1F438}]/u.exec("\uD836\uDF99"),
              ["\uD836\uDF99"]);
assertEqArray(/[\u{1D164}-\u{1F438}]/u.exec("\uD838\uDC00"),
              ["\uD838\uDC00"]);

assertEq(/[\u{1D164}-\u{1F438}]/u.exec("\uD834\uDD63"),
         null);
assertEq(/[\u{1D164}-\u{1F438}]/u.exec("\uD83D\uDC39"),
         null);

assertEq(/[\u{1D164}-\u{1F438}]/u.exec("\uD834\uE000"),
         null);
assertEq(/[\u{1D164}-\u{1F438}]/u.exec("\uD835\uDB99"),
         null);
assertEq(/[\u{1D164}-\u{1F438}]/u.exec("\uD83C\uE000"),
         null);
assertEq(/[\u{1D164}-\u{1F438}]/u.exec("\uD83D\uDB99"),
         null);

// ==== BMP + non-BMP range ====

assertEqArray(/[\u{42}-\u{1F438}]/u.exec("B"),
              ["B"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("C"),
              ["C"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uFFFF"),
              ["\uFFFF"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD800\uDC00"),
              ["\uD800\uDC00"]);

assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD800"),
              ["\uD800"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uDBFF"),
              ["\uDBFF"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uDC00"),
              ["\uDC00"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uDFFF"),
              ["\uDFFF"]);

assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uDC38"),
              ["\uDC38"]);

assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD83D\uDBFF"),
              ["\uD83D"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD83D\uDC00"),
              ["\uD83D\uDC00"]);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD83D\uDC38"),
              ["\uD83D\uDC38"]);
assertEq(/[\u{42}-\u{1F438}]/u.exec("\uD83D\uDC39"),
         null);
assertEq(/[\u{42}-\u{1F438}]/u.exec("\uD83D\uDFFF"),
         null);
assertEqArray(/[\u{42}-\u{1F438}]/u.exec("\uD83D\uE000"),
              ["\uD83D"]);

assertEq(/[\u{42}-\u{1F438}]/u.exec("A"),
         null);

// ==== wrong patterns ====

assertThrowsInstanceOf(() => eval(`/[\\u{-1}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{0.0}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{G}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{{]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{110000}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{00110000}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{100000000000000000000000000000}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{   FFFF}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{FFFF   }]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{FF   FF}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{F F F F}]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u{100000001}]/u`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
