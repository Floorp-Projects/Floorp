var list = Object.getOwnPropertyNames(this);
var found = list.indexOf("Proxy") != -1;
assertEq(found, true)
reportCompare(true, true)
