// Bug 1715471

// This test relies on triggering GC at precise points. GC zeal modes
// interfere with this.
gczeal(0);

// This test requires enqueueMark, which is only available in a debugging
// build.
if (!this.enqueueMark) quit(0);

var wm = new WeakMap();

// Force materialization of the map struct. Otherwise it would be allocated
// black during the incremental GC when we do wm.set.
wm.set({}, {});

var tenuredKey = Object.create(null);
gc(); // Tenure the WeakMap and key.

// Enter the GC, since otherwise the minor GC at the beginning would tenure our
// value before it had a chance to be entered into the ephemeron edge list.
startgc(1);
while (gcstate() === "Prepare") gcslice(1);

// Create a WeakMap entry with a value in the nursery.
var nurseryValue = Object.create(null);
wm.set(tenuredKey, nurseryValue);

// We want to mark the weakmap first, before the key or value are marked, so
// that the tenuredKey -> nurseryValue edge will be added to the ephemeron edge
// table.
enqueueMark(wm);

gcslice(1000);
minorgc();
