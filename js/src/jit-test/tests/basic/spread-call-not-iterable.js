load(libdir + "asserts.js");
load(libdir + "iteration.js");

assertThrowsInstanceOf(() => Math.sin(...true), TypeError);
assertThrowsInstanceOf(() => Math.sin(...false), TypeError);
assertThrowsInstanceOf(() => Math.sin(...new Date()), TypeError);
assertThrowsInstanceOf(() => Math.sin(...Function("")), TypeError);
assertThrowsInstanceOf(() => Math.sin(...function () {}), TypeError);
assertThrowsInstanceOf(() => Math.sin(...(x => x)), TypeError);
assertThrowsInstanceOf(() => Math.sin(...1), TypeError);
assertThrowsInstanceOf(() => Math.sin(...{}), TypeError);
var foo = {}

foo[Symbol.iterator] = 10;
assertThrowsInstanceOf(() => Math.sin(...foo), TypeError);

foo[Symbol.iterator] = function() { return undefined; };
assertThrowsInstanceOf(() => Math.sin(...foo), TypeError);

foo[Symbol.iterator] = function() { return this; };
assertThrowsInstanceOf(() => Math.sin(...foo), TypeError);

foo[Symbol.iterator] = function() { return this; };
foo.next = function() { throw 10; };
assertThrowsValue(() => Math.sin(...foo), 10);

assertThrowsInstanceOf(() => Math.sin(.../a/), TypeError);
assertThrowsInstanceOf(() => Math.sin(...new Error()), TypeError);
