s = newGlobal();
evalcx("\
    try { \
        throw StopIteration;\
    } catch(a) {\
        x = a;\
    } \
    wrap(x);\
", s);
evalcx("\
    n = x;\
", s);
