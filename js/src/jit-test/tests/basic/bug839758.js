function f() {
    try {
        evalcx("(function(){\
                    return new function(){arguments.callee.caller()}\
                })()", newGlobal())
    } catch (e) {}
}
f()
f()

