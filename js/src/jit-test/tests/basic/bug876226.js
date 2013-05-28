// |jit-test| error: SyntaxError
try {
    evaluate("throw 3", {
	newContext: new Set,
	saveFrameChain: true
    });
} catch(e) {}

evaluate("()", {
    saveFrameChain: true
});
