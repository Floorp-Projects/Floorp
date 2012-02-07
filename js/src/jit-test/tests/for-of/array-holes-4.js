// for-of on an Array does not consult the prototype chain when it encounters a hole.

var m = {1: 'FAIL'};
var a = [0, , 2, 3];
a.__proto__ = m;
var log = [];
for (var x of a)
    log.push(x);
assertEq(log[1], undefined);
assertEq(log.join(), "0,,2,3");
