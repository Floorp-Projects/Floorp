// |jit-test| allow-oom
if (getBuildConfiguration().debug === true)
    quit(0);
function f(){};
Object.defineProperty(f, "name", {value: "a".repeat((1<<28)-1)});
var ex = null;
try {
    len = f.bind().name.length;
} catch (e) {
    ex = e;
}
assertEq(ex instanceof InternalError, true);
