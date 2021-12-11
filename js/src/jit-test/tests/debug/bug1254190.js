// |jit-test| slow; skip-if: !('oomTest' in this); allow-oom

var g = newGlobal({newCompartment: true});
var dbg = new Debugger(g);
dbg.onNewScript = function (s) {
  log += dbg.findScripts({ source: s.source }).length;
}
log = "";
oomTest(() => {
    var static  = newGlobal({sameZoneAs: this});
    g.eval("(function() {})()");
});
