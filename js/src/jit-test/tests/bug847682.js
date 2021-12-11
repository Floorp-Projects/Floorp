function computeInputVariants(len) {
  var orig = "";
  var pointed = "";
  for (var i = 0; i < len; i++) {
    orig += (i%10)+"";
    pointed += (i%10)+".";
  }
  return [orig, pointed];
}

function test() {
  var re11 = /\./g;
  for (var i=0; i < 40; i++) {
    var a = computeInputVariants(i);
    assertEq(a[0], a[1].replace(re11, ''))
  }
}
test();
