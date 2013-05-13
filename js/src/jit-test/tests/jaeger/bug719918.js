function test(m) {
  do {
    if (m = arr[0]) break;
    m = 0;
  }
  while (0);
  arr[1] = m;
}

arr = new Float64Array(2);

// run function a lot to trigger methodjit compile
for(var i=0; i<200; i++)
  test(0);

// should return 0, not NaN
assertEq(arr[1], 0)
