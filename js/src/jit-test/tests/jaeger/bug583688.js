// |jit-test| error: ReferenceError
__defineSetter__("x", function () {})
try {
    __defineGetter__("d", (Function("x")))
} catch (e) {}
d
print(delete x)
throw d


