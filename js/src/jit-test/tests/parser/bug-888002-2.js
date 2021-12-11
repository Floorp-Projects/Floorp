// Constant folding doesn't affect non-strict delete.

(function (x) {
    // These senseless delete-expressions are legal. Per ES5.1 11.4.1 step 2,
    // each one does nothing and returns true.
    assertEq(delete (1 ? x : x), true);
    assertEq(delete (0 || x), true);
    assertEq(delete (1 && x), true);

    // This one is legal too, but returns false.
    assertEq(delete x, false);
}());
