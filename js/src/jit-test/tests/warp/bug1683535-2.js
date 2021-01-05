function testMathyFunction(f, inputs) {
  var results = [];
  for (var j = 0; j < inputs.length; ++j) 
    for (var k = 0; k < inputs.length; ++k) 
      results.push(f(inputs[j], inputs[k]));
}
mathy0 = (function(x, y) {
  return (Math.clz32((x <= x) >>> y) >> (~(0x080000000 >>> 0))) % Math.acos(~(2 ** 53)) >>> 0
});
testMathyFunction(mathy0, [1, 42, 0 / 0, 1 / 0, -Number.MIN_SAFE_INTEGER, -(2 ** 53), (2 ** 53), 1.7976931348623157e308]);
