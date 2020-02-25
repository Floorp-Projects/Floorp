// |jit-test| --enable-weak-refs

function checkPropertyDescriptor(obj, property, writable, enumerable,
                                 configurable) {
  let desc = Object.getOwnPropertyDescriptor(obj, property);
  assertEq(typeof desc, "object");
  assertEq(desc.writable, writable);
  assertEq(desc.enumerable, enumerable);
  assertEq(desc.configurable, configurable);
}

function assertThrowsTypeError(thunk) {
  let error;
  try {
    thunk();
  } catch (e) {
    error = e;
  }
  assertEq(error instanceof TypeError, true);
}

// 3.1 The FinalizationGroup Constructor
assertEq(typeof this.FinalizationGroup, "function");

// 3.1.1 FinalizationGroup ( cleanupCallback ) 
assertThrowsTypeError(() => new FinalizationGroup());
assertThrowsTypeError(() => new FinalizationGroup(1));
new FinalizationGroup(x => 0);

// 3.2 Properties of the FinalizationGroup Constructor
assertEq(Object.getPrototypeOf(FinalizationGroup), Function.prototype);

// 3.2.1 FinalizationGroup.prototype
checkPropertyDescriptor(FinalizationGroup, 'prototype', false, false, false);

// 3.3 Properties of the FinalizationGroup Prototype Object
let proto = FinalizationGroup.prototype;
assertEq(Object.getPrototypeOf(proto), Object.prototype);

// 3.3.1 FinalizationGroup.prototype.constructor
assertEq(proto.constructor, FinalizationGroup);

// 3.3.2 FinalizationGroup.prototype.register ( target , holdings [, unregisterToken ] )
assertEq(proto.hasOwnProperty('register'), true);
assertEq(typeof proto.register, 'function');

// 3.3.3 FinalizationGroup.prototype.unregister ( unregisterToken )
assertEq(proto.hasOwnProperty('unregister'), true);
assertEq(typeof proto.unregister, 'function');

// 3.3.4 FinalizationGroup.prototype.cleanupSome ( [ callback ] )
assertEq(proto.hasOwnProperty('cleanupSome'), true);
assertEq(typeof proto.cleanupSome, 'function');

// 3.3.5 FinalizationGroup.prototype [ @@toStringTag ]
assertEq(proto[Symbol.toStringTag], "FinalizationGroup");
checkPropertyDescriptor(proto, Symbol.toStringTag, false, false, true);

// 3.4 Properties of FinalizationGroup Instances
let group = new FinalizationGroup(x => 0);
assertEq(Object.getPrototypeOf(group), proto);
assertEq(Object.getOwnPropertyNames(group).length, 0);

// Get a cleanup iterator.
let iterator;
group = new FinalizationGroup(it => iterator = it);
group.register({}, 0);
gc();
drainJobQueue();
assertEq(typeof group, 'object');
assertEq(typeof iterator, 'object');

// 3.5.2 The %FinalizationGroupCleanupIteratorPrototype% Object
let arrayIterator = [][Symbol.iterator]();
let iteratorProto = arrayIterator.__proto__.__proto__;
proto = iterator.__proto__;
assertEq(typeof proto, "object");
assertEq(proto.__proto__, iteratorProto);

// 3.5.2.1 %FinalizationGroupCleanupIteratorPrototype%.next()
assertEq(proto.hasOwnProperty("next"), true);
assertEq(typeof proto.next, "function");

// 3.5.2.2 %FinalizationGroupCleanupIteratorPrototype% [ @@toStringTag ]
assertEq(proto[Symbol.toStringTag], "FinalizationGroup Cleanup Iterator");
checkPropertyDescriptor(proto, Symbol.toStringTag, false, false, true);

// 3.5.3 Properties of FinalizationGroup Cleanup Iterator Instances
assertEq(Object.getOwnPropertyNames(iterator).length, 0);

let heldValues = [];
group = new FinalizationGroup(iterator => {
  heldValues.push(...iterator);
});

// Test a single target.
heldValues = [];
group.register({}, 42);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 42);

// Test multiple targets.
heldValues = [];
for (let i = 0; i < 100; i++) {
  group.register({}, i);
}
gc();
drainJobQueue();
assertEq(heldValues.length, 100);
heldValues = heldValues.sort((a, b) => a - b);
for (let i = 0; i < 100; i++) {
  assertEq(heldValues[i], i);
}

// Test a single object in multiple groups
heldValues = [];
let heldValues2 = [];
let group2 = new FinalizationGroup(iterator => {
  heldValues2.push(...iterator);
});
{
  let object = {};
  group.register(object, 1);
  group2.register(object, 2);
  object = null;
}
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 1);
assertEq(heldValues2.length, 1);
assertEq(heldValues2[0], 2);

// Unregister a single target.
heldValues = [];
let token = {};
group.register({}, 1, token);
group.unregister(token);
gc();
drainJobQueue();
assertEq(heldValues.length, 0);

// Unregister multiple targets.
heldValues = [];
let token2 = {};
group.register({}, 1, token);
group.register({}, 2, token2);
group.register({}, 3, token);
group.register({}, 4, token2);
group.unregister(token);
gc();
drainJobQueue();
assertEq(heldValues.length, 2);
heldValues = heldValues.sort((a, b) => a - b);
assertEq(heldValues[0], 2);
assertEq(heldValues[1], 4);

// Watch object in another global.
let other = newGlobal({newCompartment: true});
heldValues = [];
group.register(evalcx('({})', other), 1);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 1);

// Pass heldValues from another global.
let heldValue = evalcx('{}', other);
heldValues = [];
group.register({}, heldValue);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], heldValue);

// Pass unregister token from another global.
token = evalcx('({})', other);
heldValues = [];
group.register({}, 1, token);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 1);
heldValues = [];
group.register({}, 1, token);
group.unregister(token);
gc();
drainJobQueue();
assertEq(heldValues.length, 0);

// FinalizationGroup is designed to be subclassable.
class MyGroup extends FinalizationGroup {
  constructor(callback) {
    super(callback);
  }
}
let g2 = new MyGroup(iterator => {
  heldValues.push(...iterator);
});
heldValues = [];
g2.register({}, 42);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 42);

// Test trying to use iterator after the callback.
iterator = undefined;
let g3 = new FinalizationGroup(i => iterator = i);
g3.register({}, 1);
gc();
drainJobQueue();
assertEq(typeof iterator, 'object');
assertThrowsTypeError(() => iterator.next());

// Test trying to use the wrong iterator inside the callback.
let g4 = new FinalizationGroup(x => {
  assertThrowsTypeError(() => iterator.next());
});
g4.register({}, 1);
gc();
drainJobQueue();

// Test cleanupSome.
heldValues = [];
let g5 = new FinalizationGroup(i => heldValues = [...i]);
g5.register({}, 1);
g5.register({}, 2);
g5.register({}, 3);
gc();
g5.cleanupSome();
assertEq(heldValues.length, 3);
heldValues = heldValues.sort((a, b) => a - b);
assertEq(heldValues[0], 1);
assertEq(heldValues[1], 2);
assertEq(heldValues[2], 3);

// Test trying to call cleanupSome in callback.
let g6 = new FinalizationGroup(x => {
  assertThrowsTypeError(() => g6.cleanupSome());
});
g6.register({}, 1);
gc();
drainJobQueue();

// Test that targets don't keep the finalization group alive.
let target = {};
group = new FinalizationGroup(iterator => undefined);
group.register(target, 1);
let weakRef = new WeakRef(group);
group = undefined;
assertEq(typeof weakRef.deref(), 'object');
drainJobQueue();
gc();
assertEq(weakRef.deref(), undefined);
assertEq(typeof target, 'object');

// Test that targets don't keep the finalization group alive when also
// used as the unregister token.
group = new FinalizationGroup(iterator => undefined);
group.register(target, 1, target);
weakRef = new WeakRef(group);
group = undefined;
assertEq(typeof weakRef.deref(), 'object');
drainJobQueue();
gc();
assertEq(weakRef.deref(), undefined);
assertEq(typeof target, 'object');

// Test that cleanup doesn't happen if the finalization group dies.
heldValues = [];
new FinalizationGroup(iterator => {
  heldValues.push(...iterator);
}).register({}, 1);
gc();
drainJobQueue();
assertEq(heldValues.length, 0);
