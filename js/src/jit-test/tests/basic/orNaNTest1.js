load(libdir + 'orTestHelper.js');
var orNaNTest1 = new Function("return orTestHelper(NaN, NaN, 10);");
assertEq(orNaNTest1(), 0);
