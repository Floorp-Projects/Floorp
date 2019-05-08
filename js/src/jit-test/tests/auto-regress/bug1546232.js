var g = newGlobal();
g.Int8Array.from([]);
relazifyFunctions();
g.eval("[].values()");
