if (!('oomTest' in this))
    quit();

var x = [];
oomTest(() => setGCCallback({ action: "minorGC" }));
oomTest(() => setGCCallback({ action: "majorGC" }));
