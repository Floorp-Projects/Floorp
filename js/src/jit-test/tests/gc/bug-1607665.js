var ta = new BigInt64Array(12);

var s = "";
for (var i = 0; i < ta.length; ++i) {
  var x = ta[i];
  s += x;
}

assertEq(s, "000000000000");
