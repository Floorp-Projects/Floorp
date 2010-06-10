// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

(function() {
    function f() {
        this.b = function() {};
        Object.defineProperty(this, "b", ({
            configurable: __defineSetter__("", function() {})
        }));
    }
    for each(y in [0]) {
        _ = new f();
    }
})();
uneval(this);

reportCompare(true, true);
