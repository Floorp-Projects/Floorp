// |jit-test| skip-if: !('oomTest' in this)

// Delazify a function while encoding bytecode.
oomTest(() => {
        let code = cacheEntry(`
                function f() { }
                f();
        `);
        evaluate(code, { saveIncrementalBytecode: true });
});
