// Test that we don't waste cycles on edit distance when names are too long.

const name = "a".repeat(10000);
this[name] = () => {};

let e;
try {
  eval(name + "a()");
} catch (ee) {
  e = ee;
}

assertEq(e !== undefined, true);
assertEq(e.name, "ReferenceError");
// Name was too long, shouldn't have provided a suggestion.
assertEq(e.message.contains("did you mean"), false);
