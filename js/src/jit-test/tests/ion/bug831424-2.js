// |jit-test| error: TypeError
x = [];
Object.defineProperty(this, "y", {
    get: function() {
        print.caller
    }
});
Object.defineProperty(x, 3, {
    get: function() {
        y[13];
    }
});
(function() {
    x.shift();
})();

