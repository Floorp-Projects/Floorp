Object.defineProperty(Array.prototype, "2", {
    set: function () {}
});
async function* f() {
    yield {};
}
var x = f();
x.next();
x.next();
x.next();
