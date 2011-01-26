// |jit-test| error: ReferenceError
this.__defineSetter__("x", function () {})
try {
    this.__defineGetter__("d", (Function("x")))
} catch (e) {}
d
print(delete x)
throw d


