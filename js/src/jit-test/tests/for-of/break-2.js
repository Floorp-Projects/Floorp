// A break statement leaves only a single for-of loop.

var log = '';
for (var x of [1, 2, 3]) {
    for (var y of ['.', ':']) {
        log += x + y;
        break;
    }
}
assertEq(log, "1.2.3.");
