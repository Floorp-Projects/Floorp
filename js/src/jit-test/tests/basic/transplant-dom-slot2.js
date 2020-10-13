// Make sure to transplant DOM_OBJECT_SLOT2 for FakeDOMObject
let source = new FakeDOMObject();
assertEq(source.slot, 42);
let {object: target, transplant} = transplantableObject({object: source});
transplant(this);
assertEq(source.slot, 42);
assertEq(target.slot, 42);
