// |jit-test| slow

if (!('oomTest' in this))
    quit();

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
