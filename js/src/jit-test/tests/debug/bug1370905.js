// |jit-test| allow-oom

if (!('oomTest' in this))
    quit();

var source = `
    var global = newGlobal();
    global.eval('function f() { debugger; }');
    var debug = new Debugger(global);
    var foo;
    debug.onDebuggerStatement = function(frame) {
        foo = frame.arguments[0];
        return null;
    };
    global.eval('f(0)');
`;
function test() {
    oomTest(new Function(source), false);
}
test();
