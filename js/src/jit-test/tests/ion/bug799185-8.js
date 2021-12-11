// |jit-test| error: TypeError
function testBitOrInconvertibleObjectInconvertibleObject() {
  var o1 = {};
  var count2 = 0;
  function toString2() {
        ++count2;
        if (count2 == 95) return {};
  }
  var o2 = { toString: toString2 };
  try {
    for (var i = 0; i < 100; i++)
        var q = o1 | o2;
  } catch (e)  {
    if (i !== 94)
      return gc();
    this.bar.foo ^ this
  }
}
testBitOrInconvertibleObjectInconvertibleObject()
