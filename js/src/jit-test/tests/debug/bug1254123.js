// |jit-test| error: boom

if (!('oomTest' in this))
    throw new Error("boom");

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
