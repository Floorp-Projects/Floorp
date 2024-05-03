// |jit-test| --no-threads; --baseline-warmup-threshold=1
function g(obj, v) {
    obj.prop = v;
}
function f() {
    var obj = {prop: 2};
    for (var j = 0; j < 20; j++) {}
    for (var i = 0; i < 100; i++) {
        g(/x/, 1);
        g(obj, false);
    }
}
f();
