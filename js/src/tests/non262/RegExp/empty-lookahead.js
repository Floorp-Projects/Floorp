//bug 473941
var regexp;

regexp = /(?=)/;
assertEq(regexp.test('test'), true);

if (typeof reportCompare === "function")
  reportCompare(true, true);
