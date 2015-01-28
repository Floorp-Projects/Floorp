/* test Set.prototype.forEach */

load(libdir + 'asserts.js');
load(libdir + 'iteration.js');

// testing success conditions of Set.prototype.forEach

var testSet = new Set();

function callback(value, key, set) {
    assertEq(value, key);
    testSet.add(value);
    assertEq(set.has(key), true);
}

var initialSet = new Set(['a', 1, undefined]);
initialSet.forEach(callback);

// test that both the Sets are equal and are in same order
var iterator = initialSet[Symbol.iterator]();
var count = 0;
for (var v of testSet) {
    assertEq(initialSet.has(v), true);
    assertIteratorNext(iterator, v);
    count++;
}

//check both the Sets we have are equal in size
assertEq(initialSet.size, testSet.size);
assertEq(initialSet.size, count);

var x = { abc: 'test'};
function callback2(value, key, set) {
    assertEq(x, this);
}
initialSet = new Set(['a']);
initialSet.forEach(callback2, x);

// testing failure conditions of Set.prototype.forEach

var m = new Map([['a', 1], ['b', 2.3], ['c', undefined]]);
assertThrowsInstanceOf(function() {
    Set.prototype.forEach.call(m, callback);
}, TypeError, "Set.prototype.forEach should raise TypeError if not a used on a Set");

var fn = 2;
assertThrowsInstanceOf(function() {
    initialSet.forEach(fn);
}, TypeError, "Set.prototype.forEach should raise TypeError if callback is not a function");
