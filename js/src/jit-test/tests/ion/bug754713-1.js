// Don't eliminate a phi if it has no SSA uses but its value is still
// observable in the interpreter.
var t = 100;
function foo(ox) {
    var x = ox;
    var n = 0;
    for (var i = 0; i < 90; i++) {
        n += x.f.g;
        if (i >= 80)
            t;
    }
}
function f() {}
function g() {}
f.prototype = {f:g};
foo(new f());
