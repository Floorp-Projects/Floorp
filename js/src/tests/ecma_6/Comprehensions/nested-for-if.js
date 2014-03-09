// For and if clauses can nest without limit in comprehensions.  This is
// unlike JS 1.8 comprehensions, which can only have one trailing "if"
// clause.

function* range(start, end) {
  for (var n = start; n < end; n++)
    yield n;
}

function primesBetween6And25() {
  return [for (n of range(6, 25)) if (n % 2) if (n % 3) if (n % 5) n];
}
assertDeepEq(primesBetween6And25(), [7,11,13,17,19,23]);

function countUpToEvens(limit) {
  return [for (n of range(0, limit)) if (!(n % 2)) for (m of range(0, n)) m]
}
assertDeepEq(countUpToEvens(7), [0,1,0,1,2,3,0,1,2,3,4,5]);

reportCompare(null, null, "test");
