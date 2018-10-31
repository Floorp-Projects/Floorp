// |jit-test| error: SyntaxError
new Function("if (0) import('')")();
