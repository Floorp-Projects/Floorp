// assignment to watched global properties must not be traced
var hits = 0;
function counter(id, oldval, newval) {
    hits++;
    return newval;
}

var x = 0;
var y = 0;
(function () {
    var a = ['x', 'y'];
    this.watch('z', counter);
    for (var i = 0; i < HOTLOOP + 5 + 1; i++) {
        this.watch(a[+(i > HOTLOOP)], counter);
        y = 1;
    }
})();
assertEq(hits, 5);

