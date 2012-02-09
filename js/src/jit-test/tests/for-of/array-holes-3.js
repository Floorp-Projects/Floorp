// for-of does not consult Array.prototype when it encounters a hole.

Array.prototype[1] = 'FAIL';
var log = [];
for (var x of [0, , 2, 3])
    log.push(x);
assertEq(log[1], undefined);
assertEq(log.join(), "0,,2,3");
