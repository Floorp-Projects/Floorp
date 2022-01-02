// |jit-test| skip-if: !('oomTest' in this)

evaluate(`
function ERROR(msg) {
  throw new Error("boom");
}
for (var i = 0; i < 9; ++ i) {
  var dbg = new Debugger;
  dbg.onNewGlobalObject = ERROR;
}
oomTest(function() {
  newGlobal({sameZoneAs: this});
})
`);
