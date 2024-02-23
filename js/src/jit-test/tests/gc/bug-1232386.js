// |jit-test| allow-oom

var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {
    global.seen = true;
};

oomTest(function() {
    newGlobal({sameZoneAs: this})
});
