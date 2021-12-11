s = newGlobal();
evalcx("\
    try { \
        throw StopIteration;\
    } catch(a) {\
        x = a;\
    } \
    new Proxy(x, {});\
", s);
evalcx("\
    n = x;\
", s);
