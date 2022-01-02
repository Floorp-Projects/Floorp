// |jit-test| skip-if: !('oomTest' in this)

loadFile("");
loadFile("");
loadFile("Array.prototype.splice.call(1)");
function loadFile(lfVarx) {
    parseInt("1");
    oomTest(function() {
        eval(lfVarx);
    });
}
