// |jit-test| skip-if: !('oomTest' in this); slow

function test(lfVarx) {
    try {
        oomTest(function() {
            let m41 = parseModule(lfVarx);
            moduleLink(m41);
            moduleEvaluate(m41);
        });
    } catch (lfVare) {}
}
test(`
  var g93 = newGlobal({newCompartment: true});
  g93.eval("f(10);");
`);
