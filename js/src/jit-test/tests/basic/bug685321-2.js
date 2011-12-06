var o = {};
function f() {
    function g() {
        x = 80;
        return x;
    };
    Object.defineProperty(o, "f", {get:g});
    var [x] = 0;
    x = {};
    2 + o.f;
    print(x);
}
f();
