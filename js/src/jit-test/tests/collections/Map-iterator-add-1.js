// map.iterator() is live: entries added during iteration are visited.

var map = Map();
function force(k) {
    if (!map.has(k) && k >= 0)
        map.set(k, k - 1);
}
force(5);
var log = '';
for (let [k, v] of map) {
    log += k + ';';
    force(v);
}
assertEq(log, '5;4;3;2;1;0;');
assertEq(map.size(), 6);
