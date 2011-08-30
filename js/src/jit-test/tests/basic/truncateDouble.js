
// check correctness of truncation of doubles slightly outside the int32 range.

function truncate(x) {
  return x | 0;
}

assertEq(truncate(0xffffffff), -1);
assertEq(truncate(0xffffffff + 5000.5), 4999);
assertEq(truncate(-0xffffffff - 5000.5), -4999);
