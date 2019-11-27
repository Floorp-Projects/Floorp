// |jit-test| skip-if: !('oomTest' in this)

var globals = [];
for (var i = 0; i < 24; ++i) {
    var g = newGlobal();
    globals.push(g);
}

var i = 0;
oomTest(function() {
    globals[(i++) % globals.length].eval(`
        evaluate("function f() {}", {envChainObject: this.env});
    `);
});
