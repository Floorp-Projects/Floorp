function foo()
{
  // Range analysis incorrectly computes a range for the multiplication.  Once
  // that incorrect range is computed, the goal is to compute a new value whose
  // range analysis *thinks* is in int32_t range, but which goes past it using
  // JS semantics.
  //
  // On the final iteration, in JS semantics, the multiplication produces 0, and
  // the next addition 0x7fffffff.  Adding any positive integer to that goes
  // past int32_t range: here, (0x7fffffff + 5) or 2147483652.
  //
  // Range analysis instead thinks the multiplication produces a value in the
  // range [INT32_MIN, INT32_MIN], and the next addition a value in the range
  // [-1, -1].  Adding any positive value to that doesn't overflow int32_t range
  // but *does* overflow the actual range in JS semantics.  Thus omitting
  // overflow checks produces the value 0x80000004, which interpreting as signed
  // is (INT32_MIN + 4) or -2147483644.
  //
  // For this test to trigger the bug it was supposed to trigger:
  //
  //   * 0x7fffffff must be the LHS, not RHS, of the addition in the loop, and
  //   * i must not be incremented using ++
  //
  // The first is required because JM LoopState doesn't treat *both* V + mul and
  // mul + V as not overflowing, when V is known to be int32_t -- only V + mul.
  // (JM pessimally assumes V's type might change before it's evaluated.  This
  // obviously can't happen if V is a constant, but JM's puny little mind
  // doesn't detect this possibility now.)
  //
  // The second is required because JM LoopState only ignores integer overflow
  // on multiplications if the enclosing loop is a "constrainedLoop" (the name
  // of the relevant field).  Loops become unconstrained when unhandled ops are
  // found in the loop.  Increment operators generate a DUP op, which is not
  // presently a handled op, causing the loop to become unconstrained.
  for (var i = 0; i < 15; i = i + 1) {
    var y = (0x7fffffff + ((i & 1) * -2147483648)) + 5;
  }
  return y;
}
assertEq(foo(), (0x7fffffff + ((14 & 1) * -2147483648)) + 5);

function bar()
{
  // Variation on the theme of the above test with -1 as the other half of the
  // INT32_MIN multiplication, which *should* result in -INT32_MIN on multiply
  // (exceeding int32_t range).
  //
  // Here, range analysis again thinks the range of the multiplication is
  // INT32_MIN.  We'd overflow-check except that adding zero (on the LHS, see
  // above) prevents overflow checking, so range analysis thinks the range is
  // [INT32_MIN, INT32_MIN] when -INT32_MIN is actually possible.  This direct
  // result of the multiplication is already out of int32_t range, so no need to
  // add anything to bias it outside int32_t range to get a wrong result.
  for (var i = 0; i < 17; i = i + 1) {
    var y = (0 + ((-1 + (i & 1)) * -2147483648));
  }
  return y;
}
assertEq(bar(), (0 + ((-1 + (16 & 1)) * -2147483648)));
