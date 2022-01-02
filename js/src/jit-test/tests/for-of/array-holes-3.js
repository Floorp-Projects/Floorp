// for-of consults Array.prototype when it encounters a hole.

Array.prototype[1] = 'peek';
var log = [];
for (var x of [0, , 2, 3])
    log.push(x);
assertEq(log[1], 'peek');
assertEq(log.join(), "0,peek,2,3");
