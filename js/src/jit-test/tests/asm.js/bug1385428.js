if (!('oomTest' in this))
    quit();

loadFile(`
try {
  Array.prototype.splice.call({ get length() {
    "use asm"
    function f() {}
    return f;
} });
} catch (e) {
  assertEq(e, s2, "wrong error thrown: " + e);
}
`);
function loadFile(lfVarx) {
    try {
        oomTest(new Function(lfVarx));
    } catch (lfVare) {}
}
