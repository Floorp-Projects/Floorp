
s = newGlobal()
evalcx("\
    x = new Uint8ClampedArray;\
    x.__proto__ = [];\
", s);
evalcx("\
    x[0]\
", s);
