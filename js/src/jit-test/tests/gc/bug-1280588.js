// |jit-test| skip-if: !('oomTest' in this)

var x = [];
oomTest(() => setGCCallback({ action: "minorGC" }));
oomTest(() => setGCCallback({ action: "majorGC" }));
