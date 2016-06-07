// |jit-test| error: SyntaxError
try {
    evaluate("throw 3");
} catch(e) {}

evaluate("()", {});
