var x = [];
oomTest(() => setGCCallback({ action: "minorGC" }));
oomTest(() => setGCCallback({ action: "majorGC" }));
