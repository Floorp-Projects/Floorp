// Don't assert.
try {
    eval("function x(y = {\
        x: (7) ? 0 : yield(0)\
    }");
} catch (e) {}
