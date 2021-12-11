// for-of on a slow Array consults the prototype chain when it encounters a hole.

var a = [0, , , 3];
a.slow = true;
Object.prototype[1] = 'peek1';
Array.prototype[2] = 'peek2';

var log = [];
for (var x of a)
    log.push(x);
assertEq(log[1], 'peek1');
assertEq(log[2], 'peek2');
assertEq(log.join(), "0,peek1,peek2,3");
