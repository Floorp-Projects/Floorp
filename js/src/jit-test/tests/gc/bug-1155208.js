var handler = {
    get: function get(t, pk, r) {},
    has: function has(t = has = parseInt) {}
};
Object.prototype.__proto__ = new Proxy(Object.create(null), handler);
function f() {
    var desc = { get: function() {} };
    var arr = Object.defineProperty([], "0", desc);
}
schedulegc(24);
evaluate("f()");
