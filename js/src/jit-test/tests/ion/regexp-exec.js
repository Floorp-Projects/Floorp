// RegExp.exec -> RegExp.test optimization should use the builtin test method.
function f() {
    var res = 0;
    for (var i=0; i<100; i++) {
        if (/a/.exec("a"))
            res++;
    }
    assertEq(res, 100);
}
delete RegExp.prototype.test;
gc();
f();

RegExp.prototype.test = function() { assertEq(0, 1); }
gc();
f();

Object.defineProperty(RegExp.prototype, "test", {get: function() { assertEq(0, 1); }});
gc();
f();
