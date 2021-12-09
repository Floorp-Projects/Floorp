// |jit-test| skip-if: !('oomTest' in this)
oomTest(function () {
    offThreadCompileModule('');
    finishOffThreadModule();
});
