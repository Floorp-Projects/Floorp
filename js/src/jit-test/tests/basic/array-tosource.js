load(libdir + 'asserts.js');

assertEq(Array.prototype.toSource.call([1, 'hi']), '[1, "hi"]');
assertEq(Array.prototype.toSource.call({1: 10, 0: 42, length: 2}), "[42, 10]");
assertEq(Array.prototype.toSource.call({1: 10, 0: 42, length: 1}), "[42]");
assertThrowsInstanceOf(() => Array.prototype.toSource.call("someString"), TypeError);
assertThrowsInstanceOf(() => Array.prototype.toSource.call(42), TypeError);
assertThrowsInstanceOf(() => Array.prototype.toSource.call(undefined), TypeError);
