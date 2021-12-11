// The decompiler can handle the implicit call to @@iterator in a for-of loop.

var x;
function check(code, msg) {
    var s = "no exception thrown";
    try {
        eval(code);
    } catch (exc) {
        s = exc.message;
    }

    assertEq(s, msg);
}

x = {};
check("for (var v of x) throw fit;", "x is not iterable");
check("[...x]", "x is not iterable");
check("Math.hypot(...x)", "x is not iterable");

x[Symbol.iterator] = "potato";
check("for (var v of x) throw fit;", "x is not iterable");

x[Symbol.iterator] = {};
check("for (var v of x) throw fit;", "x[Symbol.iterator] is not a function");

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
