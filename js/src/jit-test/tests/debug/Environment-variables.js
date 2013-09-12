// Comprehensive test of get/setVariable on many kinds of environments and
// bindings.

load(libdir + "asserts.js");

var cases = [
    // global bindings and bindings on the global prototype chain
    "x = VAL; @@",
    "var x = VAL; @@",
    "Object.prototype.x = VAL; @@",

    // let, catch, and comprehension bindings
    "let x = VAL; @@",
    "{ let x = VAL; @@ }",
    "let (x = VAL) { @@ }",
    "try { throw VAL; } catch (x) { @@ }",
    "try { throw VAL; } catch (x) { @@ }",
    "for (let x of [VAL]) { @@ }",
    "for each (let x in [VAL]) { @@ }",
    "switch (0) { default: let x = VAL; @@ }",
    "[function () { @@ }() for (x of [VAL])];",
    // "((function () { @@ })() for (x of [VAL])).next();",  // bug 709367

    // arguments
    "function f(x) { @@ } f(VAL);",
    "function f([w, x]) { @@ } f([0, VAL]);",
    "function f({v: x}) { @@ } f({v: VAL});",
    "function f([w, {v: x}]) { @@ } f([0, {v: VAL}]);",

    // bindings in functions
    "function f() { var x = VAL; @@ } f();",
    "function f() { let x = VAL; @@ } f();",
    "function f([x]) { let x = VAL; @@ } f(['fail']);",
    "function f(x) { { let x = VAL; @@ } } f('fail');",
    "function f() { function x() {} x = VAL; @@ } f();",

    // dynamic bindings
    "function f(s) { eval(s); @@ } f('var x = VAL');",
    "function f(s) { let (x = 'fail') { eval(s); } x = VAL; @@ } f('var x;');",
    "var x = VAL; function f(s) { eval('var x = 0;'); eval(s); @@ } f('delete x;');",
    "function f(obj) { with (obj) { @@ } } f({x: VAL});",
    "function f(obj) { with (obj) { @@ } } f(Object.create({x: VAL}));",
    "function f(b) { if (b) { function x(){} } x = VAL; @@ } f(1);",
];

var nextval = 1000;

function test(code, debugStmts, followupStmts) {
    var val = nextval++;
    var hits = 0;

    var g = newGlobal();
    g.eval("function debugMe() { var x = 'wrong-x'; debugger; }");
    g.capture = null;

    var dbg = Debugger(g);
    dbg.onDebuggerStatement = function (frame) {
        if (frame.callee !== null && frame.callee.name == 'debugMe')
            frame = frame.older;
        var env = frame.environment.find("x");
        assertEq(env.getVariable("x"), val)
        assertEq(env.setVariable("x", 'ok'), undefined);
        assertEq(env.getVariable("x"), 'ok');

        // setVariable cannot create new variables.
        assertThrowsInstanceOf(function () { env.setVariable("newVar", 0); }, TypeError);
        hits++;
    };

    code = code.replace("@@", debugStmts);
    if (followupStmts !== undefined)
        code += " " + followupStmts;
    code = code.replace(/VAL/g, uneval(val));
    g.eval(code);
    assertEq(hits, 1);
}

for (var s of cases) {
    // Test triggering the debugger right in the scope in which x is bound.
    test(s, "debugger; assertEq(x, 'ok');");

    // Test calling a function that triggers the debugger.
    test(s, "debugMe(); assertEq(x, 'ok');");

    // Test triggering the debugger from a scope nested in x's scope.
    test(s, "let (y = 'irrelevant') { (function (z) { let (zz = y) { debugger; }})(); } assertEq(x, 'ok');"),

    // Test closing over the variable and triggering the debugger later, after
    // leaving the variable's scope.
    test(s, "capture = {dbg: function () { debugger; }, get x() { return x; }};",
            "assertEq(capture.x, VAL); capture.dbg(); assertEq(capture.x, 'ok');");
}
