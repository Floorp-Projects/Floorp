// |jit-test| --ion-pruning=on

oomTest(() => {
    var g = newGlobal({sameZoneAs: this});
    g.eval("(function() {})()");
});
