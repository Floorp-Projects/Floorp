// |jit-test| error: TypeError
s = newGlobal();
evalcx("let NaN = 0;", s);
