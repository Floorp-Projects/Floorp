load(libdir + 'orTestHelper.js');
var orNaNTest2 = new Function("return orTestHelper(NaN, 1, 10);");
assertEq(orNaNTest2(), 45);
