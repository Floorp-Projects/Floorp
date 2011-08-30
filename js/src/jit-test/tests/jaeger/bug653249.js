
function testUnaryImacros() {
    function checkArg(x) {
        o = {
            valueOf: checkArg
        }
    }
    var v = 0;
    v += +toString;
    for (var i = 0; i;) {
        v += [].checkArg.checkArg;
    }
}(testUnaryImacros(), "valueOf passed, toString passed");
