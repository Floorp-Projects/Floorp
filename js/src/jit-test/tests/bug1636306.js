// |jit-test| --no-ion; skip-if: !('oomTest' in this)

oomTest(() => { eval(`
    function getCallee() { return getCallee.caller; }

    // Use trickery to get a reference to a run-once function. We avoid
    // expanding the lazy inner functions in this first invocation.
    let fn = function(x) {
        if (x) {
            // Singleton 'inner'
            let _ = function() {
                function inner() {}
            }();

            // Non-singleton 'inner'
            for (var i = 0; i < 2; i++) {
                let _ = function() {
                    function inner() {}
                }();
            }
        }

        return getCallee();
    }(false);

    // Run the lazy functions inside 'fn' this time. Run twice to check
    // delazification still works after OOM failure.
    for (var i = 0; i < 2; i++) {
        try { fn(true); }
        catch (e) { }
    }
`)});
