// |jit-test| allow-oom
if (!('oomTest' in this))
    quit();

var dbg = new Debugger;
dbg.onNewGlobalObject = function(global) {
    global.seen = true;
};

oomTest(function() {
    newGlobal({sameZoneAs: this})
});
