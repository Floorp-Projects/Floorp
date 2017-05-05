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
function check(expr, expected=expr, testStrict=true) {
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
            // Let in a switch
            Function("var x = 4; switch (x) { case 4: let o, undef;" + statement + "\ncase 6: break;}"),
            // Try-catch blocks
            Function("o", "undef", "try { let q = 4; try { let p = 4; } catch (e) {} } catch (e) {} { let o, undef; " + statement + " }"),
            // Let in for-in (uses with to prevent jit compilation: bug 942804, bug 831120 and bug 1041586)
            Function("with ({}) {} var undef, o; for (let z in [1, 2]) { " + statement + " }"),
        ];

        if (testStrict) {
            // Strict mode.
            cases.push(Function("o", "undef", "\"use strict\";\n" + statement));
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
check("ieval(undef)", "ieval(...)");
check("ieval.call()", "ieval.call(...)");
check("ieval(...[])", "ieval(...)");
check("ieval(...[undef])", "ieval(...)");
check("ieval(...[undef, undef])", "ieval(...)");
check("(o[0] = 4).foo", "o[0].foo");
// NOTE: This one produces different output in strict mode since "this" is
// undefined in that case.
check("this.x", "this.x", false);

for (let tok of ["|", "^", "&", "==", "!==", "===", "!==", "<", "<=", ">", ">=",
                 ">>", "<<", ">>>", "+", "-", "*", "/", "%"]) {
    check("o[(undef " + tok + " 4)]");
}

check("o[(!o)]");
check("o[(~o)]");
check("o[(+ o)]");
check("o[(- o)]");

check("o[(!(o + 1))]");
check("o[(~(o + 1))]");
check("o[(+ (o + 1))]");
check("o[(- (o + 1))]");

// A few one off tests
check_one("6", (function () { 6() }), " is not a function");
check_one("4", (function() { (4||eval)(); }), " is not a function");
check_one("0", (function () { Array.prototype.reverse.call('123'); }), " is read-only");
check_one("[...][Symbol.iterator](...).next(...).value",
          function () { ieval("{ let x; var [a, b, [c0, c1]] = [x, x, x]; }") }, " is undefined");
check_one("(void 1)", function() { (void 1)(); }, " is not a function");
check_one("(void o[1])", function() { var o = []; (void o[1])() }, " is not a function");

check_one("(typeof 1)", function() { (typeof 1)(); }, " is not a function");
check_one("(typeof o[1])", function() { var o = []; (typeof o[1])() }, " is not a function");

check_one("(delete foo)",
          function() { (delete foo)(); },
          " is not a function");
check_one("(delete obj.foo)",
          function() { var obj = {}; (delete obj.foo)(); },
          " is not a function");
check_one("(delete obj.foo)",
          function() { "use strict"; var obj = {}; (delete obj.foo)(); },
          " is not a function");
check_one("(delete obj[y])",
          function() { var obj = {}, y = {}; (delete obj[y])(); },
          " is not a function");
check_one("(delete obj[y])",
          function() { "use strict"; var obj = {}, y = {}; (delete obj[y])(); },
          " is not a function");

check_one("foo.apply(...)",
          function() { function foo() {} foo.apply()(); },
          " is not a function");

check_one("super(...)",
          function() {
            class X extends Object {
              constructor() {
                super()();
              }
            }
            new X();
          },
          " is not a function");
check_one("super(...)",
          function() {
            class X extends Object {
              constructor() {
                super(...[])();
              }
            }
            new X();
          },
          " is not a function");

check_one("eval(...)",
          function() { eval("")(); },
          " is not a function");
check_one("eval(...)",
          function() { "use strict"; eval("")(); },
          " is not a function");
check_one("eval(...)",
          function() { eval(...[""])(); },
          " is not a function");
check_one("eval(...)",
          function() { "use strict"; eval(...[""])(); },
          " is not a function");

check_one("(new foo(...))",
          function() { function foo() {}; new foo()(); },
          " is not a function");
check_one("(new foo(...))",
          function() { function foo() {}; new foo(...[])(); },
          " is not a function");
check_one("(new foo.x(...))",
          function() { var foo = { x: function() {} }; new foo.x()(); },
          " is not a function");
check_one("(new foo.x(...))",
          function() { var foo = { x: function() {} }; new foo.x(...[])(); },
          " is not a function");

check_one("[...].foo",
          function() { [undefined].foo(); },
          " is not a function");
check_one("[...].foo",
          function() { [undefined, ...[]].foo(); },
          " is not a function");

check_one("[...][Symbol.iterator](...).next(...).value",
          function () { var [{x}] = [null, {}]; }, " is null");
check_one("[...][Symbol.iterator](...).next(...).value",
          function () { var [{x}] = [void 0, {}]; }, " is undefined");

try {
  (function() {
    "use strict";
    var o = [];
    Object.freeze(o);
    o[0] = "foo";
  }());
  throw new Error("didn't throw");
} catch (e) {
  assertEq(e instanceof TypeError, true,
           "expected TypeError, got " + e);
  assertEq(e.message,
           "can't define array index property past the end of an array with non-writable length");
}

// Check fallback behavior
assertThrowsInstanceOf(function () { for (let x of undefined) {} }, TypeError);
