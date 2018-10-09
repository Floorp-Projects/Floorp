// |jit-test| slow; skip-if: !('oomTest' in this)

var g = newGlobal();
var dbg = new Debugger(g);
dbg.onNewScript = function (s) {
  log += dbg.findScripts({ source: s.source }).length;
}
log = "";
oomTest(() => {
    var static  = newGlobal({sameZoneAs: this});
    g.eval("(function() {})()");
});
