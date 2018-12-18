// |jit-test| error:ReferenceError
(function(global) {
    global.makeIterator = function makeIterator(overrides) {
        var iterator = {
            return: function(x) {
                return overrides.ret(x);
            }
        };
        return function() {
            return iterator;
        };
    }
})(this);
var iterable = {};
iterable[Symbol.iterator] = makeIterator({
    ret: (function() {
        enableGeckoProfilingWithSlowAssertions();
    })
});
0, [...{} [throwlhs()]] = iterable;
