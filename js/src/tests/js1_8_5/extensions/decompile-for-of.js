// The decompiler can handle the implicit call to @@iterator in a for-of loop.

var x;
function check(code) {
    var s = "no exception thrown";
    try {
        eval(code);
    } catch (exc) {
        s = exc.message;
    }

    assertEq(s, `x[Symbol.iterator] is not a function`);
}

x = {};
check("for (var v of x) throw fit;");
check("[...x]");
check("Math.hypot(...x)");

x[Symbol.iterator] = "potato";
check("for (var v of x) throw fit;");

x[Symbol.iterator] = {};
check("for (var v of x) throw fit;");

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
