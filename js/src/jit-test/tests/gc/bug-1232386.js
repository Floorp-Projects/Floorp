// |jit-test| allow-oom; skip-if: !('oomTest' in this)

var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {
    global.seen = true;
};

oomTest(function() {
    newGlobal({sameZoneAs: this})
});
