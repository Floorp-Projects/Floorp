pSandbox = newGlobal('new-compartment');
evalcx("\
    x = ArrayBuffer;\
    y = Map();\
    x += 1;\
    w = x;\
    x += '0';\
    z = x;\
", pSandbox);
evalcx("\
    x + '0';\
", pSandbox);
evalcx("\
    y.delete(z);\
    w.slice(2);\
", pSandbox)
