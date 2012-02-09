// for-of on a slow Array does not consult the prototype chain when it encounters a hole.

var a = [0, , , 3];
a.slow = true;
Object.prototype[1] = 'FAIL1';
Array.prototype[2] = 'FAIL2';

var log = [];
for (var x of a)
    log.push(x);
assertEq(log[1], undefined);
assertEq(log[2], undefined);
assertEq(log.join(), "0,,,3");
