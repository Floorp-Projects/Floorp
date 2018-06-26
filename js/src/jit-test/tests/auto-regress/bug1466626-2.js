if (!("oomTest" in this))
    quit();

var globals = [];
for (var i = 0; i < 24; ++i) {
    var g = newGlobal();
    g.eval(`
        function f(){}
        var env = {};
    `);
    globals.push(g);
}

var i = 0;
oomTest(function() {
    globals[(i++) % globals.length].eval(`
        this.clone(this.f, this.env);
    `);
});
