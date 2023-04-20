// Put DOM object on prototype chain
let x = new FakeDOMObject;
let y = Object.create(x);

// Transplant the DOM object while it is still a prototype
let g = newGlobal({newCompartment: true});
let { transplant } = transplantableObject({ object: x });

// JIT an IC to access Object.prototype.toString
function f(o) { return o.toString; }
for (var i = 0; i < 20; ++i) { f(y) }

// Transplanting should not interfere with teleporting
transplant(g);
x.toString = "override";
assertEq(f(y), "override");
