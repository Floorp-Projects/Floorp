function Obj1(x) { this.x = x; }
function f1() {
    var arr = [], o = {};
    for (var i=0; i<2500; i++) {
        arr.push(new Obj1(o));
        if (i < 15) {
            arr[i].x = undefined;
            arr[i].x = Math;
        }
    }
    for (var i=0; i<2500; i++) {
        var y = (i > 2000) ? undefined : o;
        arr[i].x = y;
    }
}
f1();

function f2() {
    var arr = [], p = {};
    for (var i=0; i<2500; i++) {
        var x = (i < 2000) ? p : undefined;
        var o = {x: x};
        if (i < 5) {
            o.x = undefined;
            o.x = p;
        }
        arr.push(o);
    }
    for (var i=0; i<2500; i++) {
        assertEq(arr[i].x, i < 2000 ? p : undefined);
    }
}
f2();

function f3() {
    var arr = [], p = {};
    for (var i=0; i<2500; i++) {
        var x = (i < 2000) ? p : true;
        var o = {x: x};
        if (i < 5) {
            o.x = true;
            o.x = p;
        }
        arr.push(o);
    }
    for (var i=0; i<2500; i++) {
        assertEq(arr[i].x, i < 2000 ? p : true);
    }
}
f3();
