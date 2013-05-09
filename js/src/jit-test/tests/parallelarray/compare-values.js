load(libdir + "parallelarray-helpers.js");

// Test that we are able to compare numbers even if the typesets are
// not "clean" because we have previously added strings and numbers.
// Also test that we distinguish between bools/numbers etc for strict
// equality but not loose equality.

function theTest() {
  var ints = range(0, 1024);
  var doubles = ints.map(v => v + 0.1);
  var bools = ints.map(v => (v % 2) == 0);
  var strings = ints.map(v => String(v));

  function looselyCompareToDoubles(e, i) {
    return doubles[i] == e;
  }
  print("doubles");
  compareAgainstArray(doubles, "map", looselyCompareToDoubles)
  print("bools");
  compareAgainstArray(bools, "map", looselyCompareToDoubles,
                      assertStructuralEq, "mixed")
  // ion bails out when converting a string to a double right now,
  // so par exec cannot proceed
  print("strings");
  compareAgainstArray(strings, "map", looselyCompareToDoubles,
                      assertStructuralEq, "disqualified")
  print("ints");
  compareAgainstArray(ints, "map", looselyCompareToDoubles)

  function strictlyCompareToDoubles(e, i) {
    return doubles[i] === e;
  }
  print("doubles, strict");
  compareAgainstArray(doubles, "map", strictlyCompareToDoubles,
                     assertStructuralEq, "mixed")
  print("bools, strict");
  compareAgainstArray(bools, "map", strictlyCompareToDoubles,
                      assertStructuralEq, "mixed")
  print("strings, strict");
  compareAgainstArray(strings, "map", strictlyCompareToDoubles,
                      assertStructuralEq, "mixed")
  print("ints, strict");
  compareAgainstArray(ints, "map", strictlyCompareToDoubles)
}

if (getBuildConfiguration().parallelJS)
  theTest();
