var NewClassPrototype = getSelfHostedValue("NewClassPrototype");
var NewObjectWithClassPrototype = getSelfHostedValue("NewObjectWithClassPrototype");
var HaveSameClass = getSelfHostedValue("HaveSameClass");
var UnsafeGetReservedSlot = getSelfHostedValue("UnsafeGetReservedSlot");
var UnsafeSetReservedSlot = getSelfHostedValue("UnsafeSetReservedSlot");

var numSlots = 5;
var C = NewClassPrototype(numSlots);

C.getSlots = function getSlots() {
  if (!HaveSameClass(this, C))
    throw TypeError("bad class");
  var slots = [];
  for (var i = 0; i < numSlots; i++)
    slots.push(UnsafeGetReservedSlot(this, i));
  return slots;
};

C.setSlots = function setSlots(slots) {
  if (!HaveSameClass(this, C))
    throw TypeError("bad class");
  for (var i = 0; i < numSlots; i++)
    UnsafeSetReservedSlot(this, i, slots[i]);
};

function f() {
  var obj = NewObjectWithClassPrototype(C);
  var before = ["foo", 2.22, ["bar", "baz"], { x: 0, y: -1.1 }, false];
  obj.setSlots(before);
  var after = obj.getSlots();

  assertEq(before.length, after.length);
  for (var i = 0; i < before.length; i++)
    assertEq(before[i], after[i]);
}

f();
