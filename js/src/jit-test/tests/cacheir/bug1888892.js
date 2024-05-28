function f() {
    try { arr.toReversed(); } catch (e) {}
    var proxy = new Proxy(Object.prototype, {get() {}});
    Object.setPrototypeOf(Array.prototype, proxy);
}
gczeal(10);
var arr = [];
Object.defineProperty(arr, 0, {get: f});
f();
