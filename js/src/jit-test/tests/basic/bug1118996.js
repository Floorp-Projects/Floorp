// |jit-test| allow-oom; --fuzzing-safe; --no-threads; --no-ion; allow-unhandlable-oom
g = newGlobal();
gcparam('maxBytes', gcparam('gcBytes'));
try {
    evaluate("return 0", ({
        global: g,
        newContext: true
    }));
} catch (error) {
    // We expect evaluate() above to fail with OOM, but due to GC zeal settings
    // it may execute correctly and throw "SyntaxError: return not in function".
    // This catch block is to ignore that error.
}
