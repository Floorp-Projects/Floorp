var getProtoCalled = false;

var newTarget = Object.defineProperty(function(){}.bind(), "prototype", {
    get() {
        getProtoCalled = true;
        return null;
    }
});

var AsyncGenerator = async function*(){}.constructor;

assertThrowsInstanceOf(() => {
    Reflect.construct(AsyncGenerator, ["@error"], newTarget);
}, SyntaxError);

assertEq(getProtoCalled, false);

if (typeof reportCompare === "function")
    reportCompare(true, true);
