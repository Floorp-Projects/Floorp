// A break statement leaves a for-of loop.

var log = '';
for (var x of ['a', 'b', 'c']) {
    log += x;
    if (x === 'b')
        break;
}
assertEq(log, "ab");
