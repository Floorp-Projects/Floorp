// |jit-test| need-for-each

try {
    (function () {
        __proto__ = [];
        for each(y in [0, 0]) {
            this.__defineGetter__("", function () {})
        }
    })()
} catch (e) {}
