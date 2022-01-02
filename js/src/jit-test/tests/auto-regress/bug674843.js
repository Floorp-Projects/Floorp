// Binary: cache/js-dbg-64-d066929dd830-linux
// Flags: -m
//
function fnSupportsArrayIndexGettersOnObjects() {
    if (fnExists(Object.defineProperty)) {
        var obj = {};
        Object.defineProperty(obj, "0", {
            get: function () {
                supportsArrayIndexGettersOnObjects = true;
            }
        });
        var res = obj[0];
    }
    return supportsArrayIndexGettersOnObjects;
}
function fnExists( /*arguments*/ ) {
    return true;
}
var ES5Harness = (function () {
    var $this = this;
    function registerTest(test) {
        if (!(test.precondition && !test.precondition())) {
            try {
                var res = test.test.call($this);
            } catch (e) {}
        }
    }
    return {
        registerTest: registerTest
    }
})();
ES5Harness.registerTest({
    test: function testcase() {
        function callbackfn(accum, val, idx, obj) {
            if (idx === 1 && val === 1) {}
        }
        var obj = {
            length: 10
        };
        Object.defineProperty(obj, "0", {
            get: function () {
                defineProperty(idx, idx, registerTest + ": }}}}}");
            },
        });
        try {
            Array.prototype.reduce.call(obj, callbackfn, "initialValue");
        } finally {}
    },
    precondition: function prereq() {
        return fnExists(Array.prototype.reduce) && fnExists(Object.defineProperty) && fnSupportsArrayIndexGettersOnObjects();
    }
});
ES5Harness.registerTest({
    test: function testcase() {
        var obj = {};
        Object.defineProperty(obj, "property", {
            configurable: new Date()
        });
    },
    precondition: function prereq() {
        return fnExists(gczeal(2));
    }
});
