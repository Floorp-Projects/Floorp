var getProtoCalled = false;

var newTarget = Object.defineProperty(function(){}.bind(), "prototype", {
    get() {
        getProtoCalled = true;
        return null;
    }
});

assertThrowsInstanceOf(() => {
    Reflect.construct(Function, ["@error"], newTarget);
}, SyntaxError);

assertEq(getProtoCalled, false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
