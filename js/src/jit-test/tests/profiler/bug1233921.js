g = newGlobal();
g.parent = this;
g.eval("new Debugger(parent).onExceptionUnwind = function () {}");
enableGeckoProfiling();
try {
    enableSingleStepProfiling();
} catch(e) {
    quit();
}
f();
f();
function $ERROR() {
    throw Error;
}
function f() {
    try {
        $ERROR()
    } catch (ex) {}
}
