var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- lead and trail pattern in RegExpUnicodeEscapeSequence in CharacterClass.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assertEqArray(/[\uD83D\uDC38]/u.exec("\uD83D\uDC38"),
              ["\uD83D\uDC38"]);
assertEq(/[\uD83D\uDC38]/u.exec("\uD83D"),
         null);
assertEq(/[\uD83D\uDC38]/u.exec("\uDC38"),
         null);

// no unicode flag
assertEqArray(/[\uD83D\uDC38]/.exec("\uD83D\uDC38"),
              ["\uD83D"]);
assertEqArray(/[\uD83D\uDC38]/.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\uD83D\uDC38]/.exec("\uDC38"),
              ["\uDC38"]);

// RegExp constructor
assertEqArray(new RegExp("[\uD83D\uDC38]", "u").exec("\uD83D\uDC38"),
              ["\uD83D\uDC38"]);
assertEq(new RegExp("[\uD83D\uDC38]", "u").exec("\uD83D"),
         null);
assertEq(new RegExp("[\uD83D\uDC38]", "u").exec("\uDC38"),
         null);

// RegExp constructor, no unicode flag
assertEqArray(new RegExp("[\uD83D\uDC38]", "").exec("\uD83D\uDC38"),
              ["\uD83D"]);
assertEqArray(new RegExp("[\uD83D\uDC38]", "").exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(new RegExp("[\uD83D\uDC38]", "").exec("\uDC38"),
              ["\uDC38"]);

// ==== lead-only ====

// match only non-surrogate pair
assertEqArray(/[\uD83D]/u.exec("\uD83D\uDBFF"),
              ["\uD83D"]);
assertEq(/[\uD83D]/u.exec("\uD83D\uDC00"),
         null);
assertEq(/[\uD83D]/u.exec("\uD83D\uDFFF"),
         null);
assertEqArray(/[\uD83D]/u.exec("\uD83D\uE000"),
              ["\uD83D"]);

// match before non-tail char
assertEqArray(/[\uD83D]/u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\uD83D]/u.exec("\uD83DA"),
              ["\uD83D"]);

// no unicode flag
assertEqArray(/[\uD83D]/.exec("\uD83D\uDBFF"),
              ["\uD83D"]);
assertEqArray(/[\uD83D]/.exec("\uD83D\uDC00"),
              ["\uD83D"]);
assertEqArray(/[\uD83D]/.exec("\uD83D\uDFFF"),
              ["\uD83D"]);
assertEqArray(/[\uD83D]/.exec("\uD83D\uE000"),
              ["\uD83D"]);
assertEqArray(/[\uD83D]/.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\uD83D]/.exec("\uD83DA"),
              ["\uD83D"]);

// ==== trail-only ====

// match only non-surrogate pair
assertEqArray(/[\uDC38]/u.exec("\uD7FF\uDC38"),
              ["\uDC38"]);
assertEq(/[\uDC38]/u.exec("\uD800\uDC38"),
         null);
assertEq(/[\uDC38]/u.exec("\uDBFF\uDC38"),
         null);
assertEqArray(/[\uDC38]/u.exec("\uDC00\uDC38"),
              ["\uDC38"]);

// match after non-lead char
assertEqArray(/[\uDC38]/u.exec("\uDC38"),
              ["\uDC38"]);
assertEqArray(/[\uDC38]/u.exec("A\uDC38"),
              ["\uDC38"]);

// no unicode flag
assertEqArray(/[\uDC38]/.exec("\uD7FF\uDC38"),
              ["\uDC38"]);
assertEqArray(/[\uDC38]/.exec("\uD800\uDC38"),
              ["\uDC38"]);
assertEqArray(/[\uDC38]/.exec("\uDBFF\uDC38"),
              ["\uDC38"]);
assertEqArray(/[\uDC38]/.exec("\uDC00\uDC38"),
              ["\uDC38"]);
assertEqArray(/[\uDC38]/.exec("\uDC38"),
              ["\uDC38"]);
assertEqArray(/[\uDC38]/.exec("A\uDC38"),
              ["\uDC38"]);

// ==== invalid trail ====

assertEqArray(/[\uD83D\u3042]*/u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\uD83D\u3042]*/u.exec("\uD83D\u3042"),
              ["\uD83D\u3042"]);
assertEqArray(/[\uD83D\u3042]*/u.exec("\uD83D\u3042\u3042\uD83D"),
              ["\uD83D\u3042\u3042\uD83D"]);

assertEqArray(/[\uD83D\u{3042}]*/u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\uD83D\u{3042}]*/u.exec("\uD83D\u3042"),
              ["\uD83D\u3042"]);
assertEqArray(/[\uD83D\u{3042}]*/u.exec("\uD83D\u3042\u3042\uD83D"),
              ["\uD83D\u3042\u3042\uD83D"]);

assertEqArray(/[\uD83DA]*/u.exec("\uD83D"),
              ["\uD83D"]);
assertEqArray(/[\uD83DA]*/u.exec("\uD83DA"),
              ["\uD83DA"]);
assertEqArray(/[\uD83DA]*/u.exec("\uD83DAA\uD83D"),
              ["\uD83DAA\uD83D"]);

// ==== wrong patterns ====

assertThrowsInstanceOf(() => eval(`/[\\u]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u0]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u00]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u000]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u000G]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\u0.00]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\u]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\u0]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\u00]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\u000]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\u000G]/u`), SyntaxError);
assertThrowsInstanceOf(() => eval(`/[\\uD83D\\u0.00]/u`), SyntaxError);

if (typeof reportCompare === "function")
    reportCompare(true, true);
