if (!('oomTest' in this))
    quit();

gczeal(0);

var x1 = [];
var x2 = [];
var x3 = [];
var x4 = [];
(function() {
    var gns = Object.getOwnPropertyNames(this);
    for (var i = 0; i < 49; ++i) {
        var gn = gns[i];
        var g = this[gn];
        if (typeof g == "function") {
            var hns = Object.getOwnPropertyNames(gn);
            for (var j = 0; j < hns.length; ++j) {
                x1.push("");
                x1.push("");
                x2.push("");
                x2.push("");
                x3.push("");
                x3.push("");
                x4.push("");
                x4.push("");
            }
        }
    }
})();
try {
    __proto__ = function(){};
} catch (e) {
    "" + e;
}
startgc(9222);
Function("\
    (function() {})();\
    oomTest(Debugger.Script);\
")();
