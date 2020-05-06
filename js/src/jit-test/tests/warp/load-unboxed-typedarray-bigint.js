var ta = new BigInt64Array([0n, 1n]);
var q = 0;
for (var i = 0; i < 10000; ++i) {
  if (ta[i&1]) q++;
}

assertEq(q, 5000);
