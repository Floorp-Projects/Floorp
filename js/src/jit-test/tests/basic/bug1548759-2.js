// |jit-test| skip-if: !('oomTest' in this)
oomTest(function() {
    return {
        x: async function() {
            y
        }(), z
    }
});
