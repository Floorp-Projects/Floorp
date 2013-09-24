function test() { return "x,y,z"; };
function testClear() {
  test().split(',');
}
loadFile("1");
loadFile("testClear();");
loadFile("2");
loadFile("gc();");
loadFile("testClear();");
loadFile("new test(0);");
function loadFile(lfVarx) {
        if (lfVarx.substr(-3) != ".js" && lfVarx.length != 1) {
            switch (lfRunTypeId) {
                case 2: new Function(lfVarx)(); break;
                default: evaluate(lfVarx); break;
            }
        } else if (!isNaN(lfVarx)) {
            lfRunTypeId = parseInt(lfVarx);
    }
}
