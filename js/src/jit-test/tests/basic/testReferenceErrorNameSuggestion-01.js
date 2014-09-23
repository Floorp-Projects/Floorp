// Test that we get a suggestion in a ReferenceError's message.

function levenshteinDistance() {}

let e;
try {
  // Note "ie" instead of "ei" in "levenshtein".
  levenshtienDistance()
} catch (ee) {
  e = ee;
}

assertEq(e !== undefined, true);
assertEq(e.name, "ReferenceError");
assertEq(e.message.contains("did you mean 'levenshteinDistance'?"), true);
