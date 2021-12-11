
// TI does not account for GETELEM accessing strings, so the GETELEM PIC must
// update type constraints according to generated stubs.
function foo(a, b) {
  for (var j = 0; j < 5; j++)
    a[b[j]] + " what";
}
var a = {a:"zero", b:"one", c:"two", d:"three", e:"four"};
var b = ["a", "b", "c", "d", "e"];
foo(a, b);
foo(a, b);
a.e = 4;
foo(a, b);
