var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- raw unicode.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assertEqArray(eval(`/\uD83D\uDC38/u`).exec("\u{1F438}"),
              ["\u{1F438}"]);

// no unicode flag
assertEqArray(eval(`/\uD83D\uDC38/`).exec("\u{1F438}"),
              ["\u{1F438}"]);

// escaped (lead)
assertEq(eval(`/\\uD83D\uDC38/u`).exec("\u{1F438}"),
         null);
assertEq(eval(`/\\u{D83D}\uDC38/u`).exec("\u{1F438}"),
         null);

// escaped (trail)
assertEq(eval(`/\uD83D\\uDC38/u`).exec("\u{1F438}"),
         null);
assertEq(eval(`/\uD83D\\u{DC38}/u`).exec("\u{1F438}"),
         null);

// escaped (lead), no unicode flag
assertEqArray(eval(`/\\uD83D\uDC38/`).exec("\u{1F438}"),
              ["\u{1F438}"]);

// escaped (trail), no unicode flag
assertEqArray(eval(`/\uD83D\\uDC38/`).exec("\u{1F438}"),
              ["\u{1F438}"]);

// ==== RegExp constructor ====

assertEqArray(new RegExp("\uD83D\uDC38", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);

// no unicode flag
assertEqArray(new RegExp("\uD83D\uDC38", "").exec("\u{1F438}"),
              ["\u{1F438}"]);

// escaped(lead)
assertEq(new RegExp("\\uD83D\uDC38", "u").exec("\u{1F438}"),
         null);
assertEq(new RegExp("\\u{D83D}\uDC38", "u").exec("\u{1F438}"),
         null);

// escaped(trail)
assertEq(new RegExp("\uD83D\\uDC38", "u").exec("\u{1F438}"),
         null);
assertEq(new RegExp("\uD83D\\u{DC38}", "u").exec("\u{1F438}"),
         null);

// escaped(lead), no unicode flag
assertEqArray(new RegExp("\\uD83D\uDC38", "").exec("\u{1F438}"),
              ["\u{1F438}"]);

// escaped(trail), no unicode flag
assertEqArray(new RegExp("\uD83D\\uDC38", "").exec("\u{1F438}"),
              ["\u{1F438}"]);

// ==== ? ====

assertEqArray(eval(`/\uD83D\uDC38?/u`).exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(eval(`/\uD83D\uDC38?/u`).exec(""),
              [""]);

assertEqArray(eval(`/\uD83D\uDC38?/u`).exec("\uD83D"),
              [""]);

// no unicode flag
assertEqArray(eval(`/\uD83D\uDC38?/`).exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEq(eval(`/\uD83D\uDC38?/`).exec(""),
         null);

assertEqArray(eval(`/\uD83D\uDC38?/`).exec("\uD83D"),
              ["\uD83D"]);

// escaped (lead)
assertEq(eval(`/\\uD83D\uDC38?/u`).exec("\u{1F438}"),
         null);
assertEq(eval(`/\\uD83D\uDC38?/u`).exec(""),
         null);

assertEqArray(eval(`/\\uD83D\uDC38?/u`).exec("\uD83D"),
              ["\uD83D"]);

// escaped (trail)
assertEq(eval(`/\uD83D\\uDC38?/u`).exec("\u{1F438}"),
         null);
assertEq(eval(`/\uD83D\\uDC38?/u`).exec(""),
         null);

assertEqArray(eval(`/\uD83D\\uDC38?/u`).exec("\uD83D"),
              ["\uD83D"]);

// escaped (lead), no unicode flag
assertEqArray(eval(`/\\uD83D\uDC38?/`).exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEq(eval(`/\\uD83D\uDC38?/`).exec(""),
         null);

assertEqArray(eval(`/\\uD83D\uDC38?/`).exec("\uD83D"),
              ["\uD83D"]);

// escaped (trail), no unicode flag
assertEqArray(eval(`/\uD83D\\uDC38?/`).exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEq(eval(`/\uD83D\\uDC38?/`).exec(""),
         null);

assertEqArray(eval(`/\uD83D\\uDC38?/`).exec("\uD83D"),
              ["\uD83D"]);

// ==== RegExp constructor, ? ====

assertEqArray(new RegExp("\uD83D\uDC38?", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEqArray(new RegExp("\uD83D\uDC38?", "u").exec(""),
              [""]);

assertEqArray(new RegExp("\uD83D\uDC38?", "u").exec("\uD83D"),
              [""]);

// no unicode flag
assertEqArray(new RegExp("\uD83D\uDC38?", "").exec("\u{1F438}"),
              ["\u{1F438}"]);
assertEq(new RegExp("\uD83D\uDC38?", "").exec(""),
         null);

assertEqArray(new RegExp("\uD83D\uDC38?", "").exec("\uD83D"),
              ["\uD83D"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
