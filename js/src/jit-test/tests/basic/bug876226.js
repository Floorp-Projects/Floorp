// |jit-test| error: SyntaxError
try {
    evaluate("throw 3", {
	newContext: new Set,
    });
} catch(e) {}

evaluate("()", {});
