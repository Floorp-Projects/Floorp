if (!("oomTest" in this))
    quit();

var g = newGlobal();

var i = 0;
oomTest(function() {
    var s = String.fromCharCode((i++) & 0xffff);
    var r = "";
    for (var j = 0; j < 1000; ++j) {
        r = s + r;
    }
    g.String.prototype.toString.call(r);
});
