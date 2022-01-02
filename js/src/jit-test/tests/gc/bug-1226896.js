// |jit-test| --ion-pruning=on; skip-if: !('oomTest' in this)

oomTest(() => {
    var g = newGlobal({sameZoneAs: this});
    g.eval("(function() {})()");
});
