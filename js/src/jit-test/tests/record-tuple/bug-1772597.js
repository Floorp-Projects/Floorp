// |jit-test| skip-if: !this.hasOwnProperty("Tuple")

gczeal(14);

var c = #["a", "b", "c"]; // Need at least 3 elements to trigger the bug
var t;

for (i = 0; i < 2; i++) {
    /*
       To trigger the bug, the calculated tenured size needs to exceed
       the size of the nursery during the previous GC. So we call Tuple.with(),
       which is implemented in C++, because most of the self-hosted Tuple
       methods allocate temporary space that increases the nursery size,
       masking the bug.
     */
    t = c.with(1, "x");
    /*
      Calling gc() manually forces `t` to be tenured. This test fails if
      the GC assumes that `t` has the same alloc kind in the nursery and
      the tenured heap, as happened in Bug 1772597.
    */
    gc();
}
