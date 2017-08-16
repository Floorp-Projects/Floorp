Object.prototype.set = function () {};
let f = async function* () {
    yield;
};
let x = f();
x.next();
x.next().then(function () {
    x.next();
});
Object.defineProperty(Array.prototype, "0", {
    get: function () {
        return 1;
    }
});
