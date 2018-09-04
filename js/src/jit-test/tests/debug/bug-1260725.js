if (!('oomTest' in this))
  quit()

var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {
  dbg.memory.takeCensus({});
};
oomTest(function() {
  newGlobal({sameZoneAs: this})
});
