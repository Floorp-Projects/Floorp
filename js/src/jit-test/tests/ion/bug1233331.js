if (typeof oomTest !== 'function')
    quit();

x = 0;
try {
    a;
    b;
} catch (e) {}
var g = newGlobal();
oomTest(function() {
    return Debugger(g);
});
eval("function g() {}");
