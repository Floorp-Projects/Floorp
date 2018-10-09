// |jit-test| skip-if: !('oomTest' in this)

oomTest(function() {
    offThreadCompileScript("");
});
"".match();
