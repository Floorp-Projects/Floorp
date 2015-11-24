// |jit-test| --ion-pgo=on

if (!('oomTest' in this))
   quit();

oomTest(() => {
    var g = newGlobal();
    g.eval("(function() {})()");
});
