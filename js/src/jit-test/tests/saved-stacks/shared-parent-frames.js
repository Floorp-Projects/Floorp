// Test that parent frames are shared when the older portions of two stacks are
// the same.

let f1, f2;

function dos() {
  f1 = saveStack();
  f2 = saveStack();
}

(function uno() {
  dos();
}());


// Different youngest frames.
assertEq(f1 == f2, false);
// However the parents should be the same.
assertEq(f1.parent, f2.parent);
