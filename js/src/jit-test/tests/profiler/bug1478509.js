// |jit-test| error: SyntaxError
enableGeckoProfiling();
s = newGlobal();
evalcx("let x;", s);
evalcx("let x;", s);
