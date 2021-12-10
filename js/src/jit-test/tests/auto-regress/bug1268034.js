// |jit-test| skip-if: !('oomTest' in this)

oomTest(function() {
    offThreadCompileToStencil("");
});
"".match();
