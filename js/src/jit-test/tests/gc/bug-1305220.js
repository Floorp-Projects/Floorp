// |jit-test| allow-oom
if (!('oomAfterAllocations' in this))
    quit();
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
    g.eval(\"\" + function f() g());\
    g.eval(\"\" + function g() h());\
    g.eval(\"(\" + function() {\
        f();\
    } + \")()\");\
", s);
