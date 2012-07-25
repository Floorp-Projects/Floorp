// for-of can be used to iterate over a Map twice.

var map = Map([['a', 0], ['b', 1], ['c', 2]]);
var log = '';

for (let i = 0; i < 2; i++) {
    for (let [k, v] of map)
        log += k + v;
    log += ';'
}
assertEq(log, 'a0b1c2;a0b1c2;');
