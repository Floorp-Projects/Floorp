// |jit-test| allow-oom; skip-if: !hasFunction.oomAfterAllocations

s = newGlobal();
evalcx("\
    gczeal(10, 2);\
    k = {\
        [Symbol]() {}\
    };\
", s);
gczeal(0);
evalcx("\
    var g = newGlobal();\
    b = new Debugger;\
    g.h = function() {\
        g.oomAfterAllocations(1);\
    };\
    g.eval(\"\" + function f() { return g(); });\
    g.eval(\"\" + function g() { return h(); });\
    g.eval(\"(\" + function() {\
        f();\
    } + \")()\");\
", s);
