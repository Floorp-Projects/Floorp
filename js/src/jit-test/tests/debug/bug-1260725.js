// |jit-test| skip-if: !('oomTest' in this)

var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {
  dbg.memory.takeCensus({});
};
oomTest(function() {
  newGlobal({sameZoneAs: this})
});
