// Ensure that convertDoubleToInt32 allows the -0 case.

// If convertDoubleToInt32 fails on the -0 case, then 0 !== -0 below,
// so it will fall through to the default case.
function test(v) {
  switch(v) {
    case 0: return;
    default: assertEq(true, false); break;
  }
}

for (var i = 0; i < 10000; i++) {
  test(i % 2 === 0 ? 0 : -0);
}
