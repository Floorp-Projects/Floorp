if (!getBuildConfiguration().parallelJS)
  quit(0);

x = [];
Array.prototype.push.call(x, 2);
y = x.mapPar(function() {});
Object.defineProperty(y, 3, {
    get: (function() {
        try {
            new Int8Array(y);
            x.reducePar(function() {});
        } catch (e) { print(e); }
    })
});
y + '';
