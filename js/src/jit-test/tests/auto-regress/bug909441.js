// |jit-test| error: too much recursion
function f(code) {
    try {
        g = Function(code)
    } catch (e) {}
    g()
}
f("\
    Object.defineProperty(this,\"x\",{\
        get: function(){\
            evaluate(\"Array(x)\",{\
                newContext:true,\
                catchTermination:(function(){})\
            })\
        }\
    })\
");
f("x");
f(")");
f("x");
