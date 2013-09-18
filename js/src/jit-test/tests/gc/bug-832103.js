//|jit-test| error:TypeError
RegExp("").exec()
Object.defineProperty(this, "x", {
    get: function() {
        return new Array
    }
})
Object.defineProperty(this, "y", {
    get: function() {
        return [function() {}, 0, 0, 0, 0, 0, 0]
    }
})
r = RegExp("");
uneval(undefined)
with({
    b: gczeal(9, 2)
});
r = /()/;
y.sort(function(j) {
    if (j) {
        a =
        new
        Array
    } else {
        x.v()
    }
})
