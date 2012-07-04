// Nested for-of loops can iterate over a Map.

var map = Map([['a', 0], ['b', 1]]);
var log = '';
for (let [k0, v0] of map) {
    log += k0 + v0 + ':'
    for (let [k1, v1] of map)
        log += k1 + v1;
    log += ';'
};
assertEq(log, 'a0:a0b1;b1:a0b1;');
