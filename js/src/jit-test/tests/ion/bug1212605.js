// |jit-test| error: SyntaxError
s = newGlobal();
evalcx("let NaN = 0;", s);
