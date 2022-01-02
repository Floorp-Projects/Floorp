x = x = "";
function Obj1(x) {
    this.x = x;
}
function f() {
    var o = {};
    for (var i = 0; i < 1500; i++)
        new Obj1(o);
    Obj1('');
}
f();
