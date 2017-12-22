var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- braced pattern in RegExpUnicodeEscapeSequence.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assertEqArray(/\u{41}/u.exec("ABC"),
              ["A"]);
assertEqArray(/\u{41}/.exec("ABC" + "u".repeat(41)),
              ["u".repeat(41)]);

assertEqArray(/\u{4A}/u.exec("JKL"),
              ["J"]);
assertEqArray(/\u{4A}/.exec("JKLu{4A}"),
              ["u{4A}"]);

assertEqArray(/\u{1F438}/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/\u{1F438}/.exec("u{1F438}"),
              ["u{1F438}"]);

assertEqArray(/\u{0}/u.exec("\u{0}"),
              ["\u{0}"]);
assertEqArray(/\u{10FFFF}/u.exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);
assertEqArray(/\u{10ffff}/u.exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);

// leading 0
assertEqArray(/\u{0000000000000000000000}/u.exec("\u{0}"),
              ["\u{0}"]);
assertEqArray(/\u{000000000000000010FFFF}/u.exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);

// RegExp constructor
assertEqArray(new RegExp("\\u{0}", "u").exec("\u{0}"),
              ["\u{0}"]);
assertEqArray(new RegExp("\\u{41}", "u").exec("ABC"),
              ["A"]);
assertEqArray(new RegExp("\\u{1F438}", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(new RegExp("\\u{10FFFF}", "u").exec("\u{10FFFF}"),
              ["\u{10FFFF}"]);

assertEqArray(new RegExp("\\u{0000000000000000}", "u").exec("\u{0}"),
              ["\u{0}"]);

assertEqArray(eval(`/\\u{${"0".repeat(Math.pow(2, 24)) + "1234"}}/u`).exec("\u{1234}"),
              ["\u{1234}"]);
assertEqArray(new RegExp(`\\u{${"0".repeat(Math.pow(2, 24)) + "1234"}}`, "u").exec("\u{1234}"),
              ["\u{1234}"]);

// ==== ? ====

assertEqArray(/\u{1F438}?/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/\u{1F438}?/u.exec(""),
              [""]);

// lead-only target
assertEqArray(/\u{1F438}?/u.exec("\uD83D"),
              [""]);

// RegExp constructor
assertEqArray(new RegExp("\\u{1F438}?", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(new RegExp("\\u{1F438}?", "u").exec(""),
              [""]);
assertEqArray(new RegExp("\\u{1F438}?", "u").exec("\uD83D"),
              [""]);

// ==== + ====

assertEqArray(/\u{1F438}+/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/\u{1F438}+/u.exec("\u{1F438}\u{1F438}"),
              ["\u{1F438}\u{1F438}"]);
assertEq(/\u{1F438}+/u.exec(""),
         null);

// lead-only target
assertEq(/\u{1F438}+/u.exec("\uD83D"),
         null);
assertEqArray(/\u{1F438}+/u.exec("\uD83D\uDC38\uDC38"),
              ["\uD83D\uDC38"]);

// ==== * ====

assertEqArray(/\u{1F438}*/u.exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(/\u{1F438}*/u.exec("\u{1F438}\u{1F438}"),
              ["\u{1F438}\u{1F438}"]);
assertEqArray(/\u{1F438}*/u.exec(""),
              [""]);

// lead-only target
assertEqArray(/\u{1F438}*/u.exec("\uD83D"),
              [""]);
assertEqArray(/\u{1F438}*/u.exec("\uD83D\uDC38\uDC38"),
              ["\uD83D\uDC38"]);

// ==== lead-only ====

// match only non-surrogate pair
assertEqArray(/\u{D83D}/u.exec("\uD83D\uDBFF"),
              ["\uD83D"]);
assertEq(/\u{D83D}/u.exec("\uD83D\uDC00"),
         null);
assertEq(/\u{D83D}/u.exec("\uD83D\uDFFF"),
         null);
assertEqArray(/\u{D83D}/u.exec("\uD83D\uE000"),
              ["\uD83D"]);

// match before non-tail char
assertEqArray(/\u{D83D}/u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/\u{D83D}/u.exec("\uD83DA"),
              ["\uD83D"]);

// ==== trail-only ====

// match only non-surrogate pair
assertEqArray(/\u{DC38}/u.exec("\uD7FF\uDC38"),
              ["\uDC38"]);
assertEq(/\u{DC38}/u.exec("\uD800\uDC38"),
         null);
assertEq(/\u{DC38}/u.exec("\uDBFF\uDC38"),
         null);
assertEqArray(/\u{DC38}/u.exec("\uDC00\uDC38"),
              ["\uDC38"]);

// match after non-lead char
assertEqArray(/\u{DC38}/u.exec("\uDC38"),
              ["\uDC38"]);
assertEqArray(/\u{DC38}/u.exec("A\uDC38"),
              ["\uDC38"]);

// ==== wrong patterns ====

assertThrowsInstanceOf(() => eval(`/\\u{-1}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{0.0}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{G}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{{/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{110000}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{00110000}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{100000000000000000000000000000}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{FFFFFFFFFFFFFFFFFFFFFFFFFFFFFF}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{   FFFF}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{FFFF   }/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{FF   FF}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{F F F F}/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/\\u{100000001}/u`), SyntaxError);

// surrogate pair with braced
assertEq(/\u{D83D}\u{DC38}+/u.exec("\uD83D\uDC38\uDC38"),
         null);
assertEq(/\uD83D\u{DC38}+/u.exec("\uD83D\uDC38\uDC38"),
         null);
assertEq(/\u{D83D}\uDC38+/u.exec("\uD83D\uDC38\uDC38"),
         null);

if (typeof reportCompare === "function")
    reportCompare(true, true);
