// Test interactions between ES6 generators and not-yet-standard
// features.

function assertSyntaxError(str) {
    var msg;
    var evil = eval;
    try {
        // Non-direct eval.
        evil(str);
    } catch (exc) {
        if (exc instanceof SyntaxError)
            return;
        msg = "Assertion failed: expected SyntaxError, got " + exc;
    }
    if (msg === undefined)
        msg = "Assertion failed: expected SyntaxError, but no exception thrown";
    throw new Error(msg + " - " + str);
}

// Destructuring binding.
assertSyntaxError("function* f(x = yield) {}");
assertSyntaxError("function* f(x = yield 17) {}");
assertSyntaxError("function* f([yield]) {}");
assertSyntaxError("function* f({ yield }) {}");
assertSyntaxError("function* f(...yield) {}");

// For each.
assertSyntaxError("for yield");
assertSyntaxError("for yield (;;) {}");
assertSyntaxError("for yield (x of y) {}");
assertSyntaxError("for yield (var i in o) {}");

// Expression bodies.
assertSyntaxError("function* f() yield 7");
