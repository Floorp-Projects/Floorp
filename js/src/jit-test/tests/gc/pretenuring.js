// Test nursery string allocation and pretenuring.

gczeal(0);

gcparam("minNurseryBytes", 4096 * 1024);
gcparam("maxNurseryBytes", 4096 * 1024);
gc();

// String allocation in the nursery is initially enabled.
assertEq(nurseryStringsEnabled(), true);

// Literal strings are atoms (which are always tenured).
assertEq(isNurseryAllocated("foo"), false);

// The result of Number.toString is nursery allocated.
assertEq(isNurseryAllocated((1234).toString()), true);

// Ropes are nursery allocated.
let s = "bar";
assertEq(isNurseryAllocated("foo" + s), true);

// Dependent strings are nursery allocated.
assertEq(isNurseryAllocated("foobar".substr(1)), true);

// The testing function 'newString' allows control over which heap is used.
assertEq(isNurseryAllocated(newString("foobar", { tenured: true })), false);
assertEq(isNurseryAllocated(newString("foobar", { tenured: false })), true);

// Allocating lots of strings which survive nursery collection disables
// allocating strings in the nursery.
let a = [];
for (let i = 1; i < 500000; i++) {
  a.push(i.toString());
}
gc();
assertEq(nurseryStringsEnabled(), false);
a = undefined;

// When a large number of tenured strings die very soon after allocation,
// pretenuring for that zone is reset and nursery allocation enabled again.

gc();
assertEq(nurseryStringsEnabled(), false);
let initGCNumber = gcparam('majorGCNumber');
while (!nurseryStringsEnabled() && gcparam('majorGCNumber') < initGCNumber + 3) {
  for (let i = 1; i < 100000; i++) {
    a = i.toString();
  }
}
assertEq(nurseryStringsEnabled(), true);
