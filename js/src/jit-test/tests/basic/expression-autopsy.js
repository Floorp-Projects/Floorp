load(libdir + "asserts.js");
load(libdir + "iteration.js");

function check_one(expected, f, err) {
    var failed = true;
    try {
        f();
        failed = false;
    } catch (ex) {
        var s = ex.toString();
        assertEq(s.slice(0, 11), "TypeError: ");
        assertEq(s.slice(-err.length), err, "" + f);
        assertEq(s.slice(11, -err.length), expected);
    }
    if (!failed)
        throw new Error("didn't fail");
}
ieval = eval;
function check(expr, expected=expr) {
    var end, err;
    for ([end, err] of [[".random_prop", " is undefined"], ["()", " is not a function"]]) {
        var statement = "o = {};" + expr + end, f;
        var cases = [
            // Global scope
            function () {
                ieval("var o, undef;\n" + statement);
            },
            // Function scope
            Function("o", "undef", statement),
            // Function scope with variables
            Function("var o, undef;\n" + statement),
            // Function scope with some different arugments
            Function("arg1", "arg2", "var o, undef;\n" + statement),
            // Deoptimized function scope
            Function("o", "undef", "with (Object) {}\n" + statement),
            // Inside with
            Function("with (Object) { " + statement + " }"),
            // Closure
            Function("o", "undef", "function myfunc() { return o + undef; }\n" + statement),
            // Let definitions in a block
            Function("{ let o, undef;\n" + statement + "}"),
            // Let block
            Function("let (o, undef) { " + statement + " }"),
            // Let block with some other variables
            Function("var v1, v2; let (o, undef) { " + statement + " }"),
            // Shadowed let block
            Function("o", "undef", "let (o, undef) { " + statement + " }"),
            // Let in a switch
            Function("var x = 4; switch (x) { case 4: let o, undef;" + statement + "\ncase 6: break;}"),
            // The more lets the merrier
            Function("let (x=4, y=5) { x + y; }\nlet (a, b, c) { a + b - c; }\nlet (o, undef) {" + statement + " }"),
            // Let destructuring
            Function("o", "undef", "let ([] = []) {} let (o, undef) { " + statement + " }"),
            // Try-catch blocks
            Function("o", "undef", "try { let q = 4; try { let p = 4; } catch (e) {} } catch (e) {} let (o, undef) { " + statement + " }")
        ];

        try {
            // Let in for-in
            check_one(expected,
                      Function("var undef, o; for (let z in [1, 2]) { " + statement + " }"),
                      err);
        } catch (ex) {
            // Bug 831120.  See bug 942804 comment 5.
            if (expected == 'undef' && err == ' is undefined')
                check_one(expected + end,
                          Function("var undef, o; for (let z in [1, 2]) { " + statement + " }"),
                          err);
            else
                throw ex;
        }

        for (var f of cases) {
            check_one(expected, f, err);
        }
    }
}

check("undef");
check("o.b");
check("o.length");
check("o[true]");
check("o[false]");
check("o[null]");
check("o[0]");
check("o[1]");
check("o[3]");
check("o[256]");
check("o[65536]");
check("o[268435455]");
check("o['1.1']");
check("o[4 + 'h']", "o['4h']");
check("this.x");
check("ieval(undef)", "ieval(...)");
check("ieval.call()", "ieval.call(...)");
check("ieval(...[])", "ieval(...)");
check("ieval(...[undef])", "ieval(...)");
check("ieval(...[undef, undef])", "ieval(...)");

for (let tok of ["|", "^", "&", "==", "!==", "===", "!==", "<", "<=", ">", ">=",
                 ">>", "<<", ">>>", "+", "-", "*", "/", "%"]) {
    check("o[(undef " + tok + " 4)]");
}

check("o[!(o)]");
check("o[~(o)]");
check("o[+ (o)]");
check("o[- (o)]");


// A few one off tests
check_one("6", (function () { 6() }), " is not a function");
check_one("0", (function () { Array.prototype.reverse.call('123'); }), " is read-only");
check_one("(intermediate value)[Symbol.iterator](...).next(...).value",
          function () { ieval("let (x) { var [a, b, [c0, c1]] = [x, x, x]; }") }, " is undefined");
check_one("void 1", function() { (void 1)(); }, " is not a function");
check_one("void o[1]", function() { var o = []; (void o[1])() }, " is not a function");

// Manual testing for this case: the only way to trigger an error is *not* on
// an attempted property access during destructuring, and the error message
// invoking ToObject(null) is different: "can't convert {0} to object".
try
{
  (function() {
    var [{x}] = [null, {}];
   })();
  throw new Error("didn't throw");
}
catch (e)
{
  assertEq(e instanceof TypeError, true,
           "expected TypeError, got " + e);
  assertEq(e.message, "can't convert null to object");
}

// Check fallback behavior
assertThrowsInstanceOf(function () { for (let x of undefined) {} }, TypeError);
