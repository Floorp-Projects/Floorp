// Annex B.3.3.1 disallows Annex B lexical function behavior when redeclaring a
// parameter.

(function(f) {
  if (true) function f() {  }
  assertEq(f, 123);
}(123));

(function(f) {
  { function f() {  } }
  assertEq(f, 123);
}(123));


if (typeof reportCompare === "function")
  reportCompare(true, true);
