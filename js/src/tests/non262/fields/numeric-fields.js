// Number field names.
class C {
    128 = class {};
}
assertEq(new C()[128].name, "128");

// Bigint field names.
class D {
    128n = class {};
}
assertEq(new D()[128].name, "128");

if (typeof reportCompare === "function")
  reportCompare(true, true);
