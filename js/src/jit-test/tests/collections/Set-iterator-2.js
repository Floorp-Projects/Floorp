// Nested for-of loops can iterate over a Set.

var map = Set(['a', 'b']);
var log = '';
for (let x of map) {
    log += x + ':'
    for (let y of map)
        log += y;
    log += ';'
};
assertEq(log, 'a:ab;b:ab;');
