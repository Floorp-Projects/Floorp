// for-of on an Array consults the prototype chain when it encounters a hole.

load(libdir + "iteration.js");

var m = {1: 'peek'};
var a = [0, , 2, 3];
a.__proto__ = m;
var log = [];
Object.prototype[std_iterator] = Array.prototype[std_iterator];
for (var x of a)
    log.push(x);
assertEq(log[1], 'peek');
assertEq(log.join(), "0,peek,2,3");
