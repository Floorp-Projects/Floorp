// Test written by André Bargull for bug 1380953.
var q = 0;
function fn() {}
var newTarget = Object.defineProperty(fn.bind(), "prototype", {
    get() {
        ++q;
        return null;
    }
});
for (var i = 0; i < 100; ++i) {
    Reflect.construct(fn, [], newTarget);
}
assertEq(q, 100);
