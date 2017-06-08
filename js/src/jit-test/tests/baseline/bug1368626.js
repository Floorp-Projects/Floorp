var sandbox = evalcx("lazy");

// Ensure we can't change the "lazy" property of the sandbox to an accessor,
// because that'd allow to execute arbitrary side-effects when calling the
// resolve hook of the sandbox.
var err;
try {
    Object.defineProperty(sandbox, "lazy", {
        get() {
            Object.defineProperty(sandbox, "foo", { value: 0 });
        }
    });
} catch (e) {
    err = e;
}
assertEq(err instanceof TypeError, true);

// Don't assert here.
sandbox.foo = 1;
