var o1 = {x: {}};
function f() {
    var o = o1;
    for(var i=0; i<10; i++) {
        o1 = o.x;
    }
}
f();
