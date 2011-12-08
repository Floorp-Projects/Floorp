// assignment to watched global properties must not be traced
var hits = 0;
function counter(id, oldval, newval) {
    hits++;
    return newval;
}

var x = 0;
var y = 0;
function f() {
    var a = [{}, this];
    for (var i = 0; i < 14; i++) {
        print(shapeOf(this));
        Object.prototype.watch.call(a[+(i > 8)], "y", counter);
        y++;
    }
}
f();
assertEq(hits, 5);

