var getProtoCalled = false;

var newTarget = Object.defineProperty(function(){}.bind(), "prototype", {
    get() {
        getProtoCalled = true;
        return null;
    }
});

var Generator = function*(){}.constructor;

assertThrowsInstanceOf(() => {
    Reflect.construct(Generator, ["#error"], newTarget);
}, SyntaxError);

assertEq(getProtoCalled, false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
