// mapiter.next() returns an actual array.

var key = {};
var map = Map([[key, 'value']]);
var entry = map.iterator().next();
assertEq(Array.isArray(entry), true);
assertEq(Object.getPrototypeOf(entry), Array.prototype);
assertEq(Object.isExtensible(entry), true);

assertEq(entry.length, 2);
var names = Object.getOwnPropertyNames(entry).sort();
assertEq(names.length, 3);
assertEq(names.join(","), "0,1,length");
assertEq(entry[0], key);
assertEq(entry[1], 'value');
