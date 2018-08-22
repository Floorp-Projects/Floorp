var lfLogBuffer = `
  function testOuterForInVar() {
    return eval("for (var x in {}); (function() { return delete x; })");
  }
  testOuterForInVar();
`;
loadFile(lfLogBuffer);
loadFile(lfLogBuffer);
function loadFile(lfVarx) {
    try {
        oomTest(function() {
            eval(lfVarx);
        });
    } catch (lfVare) {}
}
