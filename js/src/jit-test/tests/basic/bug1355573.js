// |jit-test| skip-if: getBuildConfiguration("debug") === true
function f(){};
Object.defineProperty(f, "name", {value: "a".repeat((1<<30)-2)});
var ex = null;
try {
    len = f.bind().name.length;
} catch (e) {
    ex = e;
}
assertEq(ex === "out of memory" || (ex instanceof InternalError), true);
