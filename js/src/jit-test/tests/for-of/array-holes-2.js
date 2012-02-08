// for-of does not consult Object.prototype when it encounters a hole.

Object.prototype[1] = 'FAIL';
var log = [];
for (var x of [0, , 2, 3])
    log.push(x);
assertEq(log[1], undefined);
assertEq(log.join(), "0,,2,3");
