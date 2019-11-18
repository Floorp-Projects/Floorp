var g = newGlobal();
g.mainGlobal = this;

if (typeof isSameCompartment !== "function") {
  var isSameCompartment = SpecialPowers.Cu.getJSTestingFunctions().isSameCompartment;
}

var next = async function*(){}.prototype.next;

var f = g.eval(`(async function*() {
  var x = yield {message: "yield"};

  // Input completion values are correctly wrapped into |f|'s compartment.
  assertEq(isSameCompartment(x, mainGlobal), true);
  assertEq(x.message, "continue");

  return {message: "return"};
})`);

var it = f();

// The async iterator is same-compartment with |f|.
assertEq(isSameCompartment(it, f), true);

var p1 = next.call(it, {message: "initial yield"});

// The promise object is same-compartment with |f|.
assertEq(isSameCompartment(p1, f), true);

// Note: This doesn't follow the spec, which requires that only |p1 instanceof Promise| is true.
assertEq(p1 instanceof Promise || p1 instanceof g.Promise, true);

p1.then(v => {
  // The iterator result object is same-compartment with |f|.
  assertEq(isSameCompartment(v, f), true);
  assertEq(v.done, false);

  assertEq(isSameCompartment(v.value, f), true);
  assertEq(v.value.message, "yield");
});

var p2 = next.call(it, {message: "continue"});

// The promise object is same-compartment with |f|.
assertEq(isSameCompartment(p2, f), true);

// Note: This doesn't follow the spec, which requires that only |p2 instanceof Promise| is true.
assertEq(p2 instanceof Promise || p2 instanceof g.Promise, true);

p2.then(v => {
  // The iterator result object is same-compartment with |f|.
  assertEq(isSameCompartment(v, f), true);
  assertEq(v.done, true);

  assertEq(isSameCompartment(v.value, f), true);
  assertEq(v.value.message, "return");
});

var p3 = next.call(it, {message: "already finished"});

// The promise object is same-compartment with |f|.
assertEq(isSameCompartment(p3, f), true);

// Note: This doesn't follow the spec, which requires that only |p3 instanceof Promise| is true.
assertEq(p3 instanceof Promise || p3 instanceof g.Promise, true);

p3.then(v => {
  // The iterator result object is same-compartment with |f|.
  assertEq(isSameCompartment(v, f), true);
  assertEq(v.done, true);
  assertEq(v.value, undefined);
});

var p4 = next.call({}, {message: "bad |this| argument"});

// The promise object is same-compartment with |next|.
assertEq(isSameCompartment(p4, next), true);

// Only in this case we're following the spec and are creating the promise object
// in the correct realm.
assertEq(p4 instanceof Promise, true);

p4.then(() => {
  throw new Error("expected a TypeError");
}, e => {
  assertEq(e instanceof TypeError, true);
});

if (typeof reportCompare === "function")
  reportCompare(0, 0);
