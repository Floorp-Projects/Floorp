var BUGNUMBER = 1135377;
var summary = "Implement RegExp unicode flag -- raw unicode.";

print(BUGNUMBER + ": " + summary);

// ==== standalone ====

assertEqArray(eval(`/[\uD83D\uDC38]/u`).exec("\u{1F438}"),
              ["\u{1F438}"]);

// no unicode flag
assertEqArray(eval(`/[\uD83D\uDC38]/`).exec("\u{1F438}"),
              ["\uD83D"]);

// escaped (lead)
assertEq(eval(`/[\\uD83D\uDC38]/u`).exec("\u{1F438}"),
         null);
assertEq(eval(`/[\\u{D83D}\uDC38]/u`).exec("\u{1F438}"),
         null);

// escaped (trail)
assertEq(eval(`/[\uD83D\\uDC38]/u`).exec("\u{1F438}"),
         null);
assertEq(eval(`/[\uD83D\\u{DC38}]/u`).exec("\u{1F438}"),
         null);

// escaped (lead), no unicode flag
assertEqArray(eval(`/[\\uD83D\uDC38]/`).exec("\u{1F438}"),
              ["\uD83D"]);

// escaped (trail), no unicode flag
assertEqArray(eval(`/[\uD83D\\uDC38]/`).exec("\u{1F438}"),
              ["\uD83D"]);

// ==== RegExp constructor ====

assertEqArray(new RegExp("[\uD83D\uDC38]", "u").exec("\u{1F438}"),
              ["\u{1F438}"]);

// no unicode flag
assertEqArray(new RegExp("[\uD83D\uDC38]", "").exec("\u{1F438}"),
              ["\uD83D"]);

// escaped(lead)
assertEq(new RegExp("[\\uD83D\uDC38]", "u").exec("\u{1F438}"),
         null);
assertEq(new RegExp("[\\u{D83D}\uDC38]", "u").exec("\u{1F438}"),
         null);

// escaped(trail)
assertEq(new RegExp("[\uD83D\\uDC38]", "u").exec("\u{1F438}"),
         null);
assertEq(new RegExp("[\uD83D\\u{DC38}]", "u").exec("\u{1F438}"),
         null);

// escaped(lead), no unicode flag
assertEqArray(new RegExp("[\\uD83D\uDC38]", "").exec("\u{1F438}"),
              ["\uD83D"]);

// escaped(trail), no unicode flag
assertEqArray(new RegExp("[\uD83D\\uDC38]", "").exec("\u{1F438}"),
              ["\uD83D"]);

if (typeof reportCompare === "function")
    reportCompare(true, true);
