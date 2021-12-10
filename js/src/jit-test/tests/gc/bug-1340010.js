// |jit-test| skip-if: helperThreadCount() === 0 || !('deterministicgc' in this)
gczeal(0);

gc();
function weighted(wa) {
    var a = [];
    for (var i = 0; i < 30; ++i) {
        for (var j = 0; j < 20; ++j) {
            a.push(0);
        }
    }
    return a;
}
var statementMakers = weighted();
if (typeof oomTest == "function") {
    statementMakers = statementMakers.concat([function (d, b) {
        return "oomTest(" + makeFunction(d - 1, b) + ")";
    }, ]);
}
deterministicgc(true);
startgc(9469, "shrinking");
offThreadCompileToStencil("");
