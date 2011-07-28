// assignments to watched properties via ++ must not be traced
var hits = 0;
function counter(id, oldval, newval) {
    hits++;
    return newval;
}

(function () {
    var obj = {x: 0, y: 0};
    var a = ['x', 'y'];
    obj.watch('z', counter);
    for (var i = 0; i < HOTLOOP + 5 + 1; i++) {
        obj.watch(a[+(i > HOTLOOP)], counter);
        obj.y++;
    }
})();
assertEq(hits, 5);

