// Array.of does not trigger prototype setters.
// (It defines elements rather than assigning to them.)

var status = "pass";
Object.defineProperty(Array.prototype, "0", {set: v => status = "FAIL 1"});
assertEq(Array.of(1)[0], 1);
assertEq(status, "pass");

function Bag() {}
Bag.of = Array.of;
Object.defineProperty(Bag.prototype, "0", {set: v => status = "FAIL 2"});
assertEq(Bag.of(1)[0], 1);
assertEq(status, "pass");
