// |jit-test| skip-if: !this.hasOwnProperty("Tuple")

gczeal(10); // Run incremental GC in many slices

var c = ["a", "b"];
var t = Tuple.from(c);

for (i = 0; i < 100; i++) {
c = ["a", "b"];
t = Tuple.from(c);
c = null;
gc();
}
