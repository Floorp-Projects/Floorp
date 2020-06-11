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

// 3.1 The FinalizationRegistry Constructor
assertEq(typeof this.FinalizationRegistry, "function");

// 3.1.1 FinalizationRegistry ( cleanupCallback ) 
assertThrowsTypeError(() => new FinalizationRegistry());
assertThrowsTypeError(() => new FinalizationRegistry(1));
new FinalizationRegistry(x => 0);

// 3.2 Properties of the FinalizationRegistry Constructor
assertEq(Object.getPrototypeOf(FinalizationRegistry), Function.prototype);

// 3.2.1 FinalizationRegistry.prototype
checkPropertyDescriptor(FinalizationRegistry, 'prototype', false, false, false);

// 3.3 Properties of the FinalizationRegistry Prototype Object
let proto = FinalizationRegistry.prototype;
assertEq(Object.getPrototypeOf(proto), Object.prototype);

// 3.3.1 FinalizationRegistry.prototype.constructor
assertEq(proto.constructor, FinalizationRegistry);

// 3.3.2 FinalizationRegistry.prototype.register ( target , holdings [, unregisterToken ] )
assertEq(proto.hasOwnProperty('register'), true);
assertEq(typeof proto.register, 'function');

// 3.3.3 FinalizationRegistry.prototype.unregister ( unregisterToken )
assertEq(proto.hasOwnProperty('unregister'), true);
assertEq(typeof proto.unregister, 'function');

// 3.3.4 FinalizationRegistry.prototype.cleanupSome ( [ callback ] )
assertEq(proto.hasOwnProperty('cleanupSome'), true);
assertEq(typeof proto.cleanupSome, 'function');

// 3.3.5 FinalizationRegistry.prototype [ @@toStringTag ]
assertEq(proto[Symbol.toStringTag], "FinalizationRegistry");
checkPropertyDescriptor(proto, Symbol.toStringTag, false, false, true);

// 3.4 Properties of FinalizationRegistry Instances
let registry = new FinalizationRegistry(x => 0);
assertEq(Object.getPrototypeOf(registry), proto);
assertEq(Object.getOwnPropertyNames(registry).length, 0);

let heldValues = [];
registry = new FinalizationRegistry(value => {
  heldValues.push(value);
});

// Test a single target.
heldValues = [];
registry.register({}, 42);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 42);

// Test multiple targets.
heldValues = [];
for (let i = 0; i < 100; i++) {
  registry.register({}, i);
}
gc();
drainJobQueue();
assertEq(heldValues.length, 100);
heldValues = heldValues.sort((a, b) => a - b);
for (let i = 0; i < 100; i++) {
  assertEq(heldValues[i], i);
}

// Test a single object in multiple registries
heldValues = [];
let heldValues2 = [];
let registry2 = new FinalizationRegistry(value => {
  heldValues2.push(value);
});
{
  let object = {};
  registry.register(object, 1);
  registry2.register(object, 2);
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
registry.register({}, 1, token);
registry.unregister(token);
gc();
drainJobQueue();
assertEq(heldValues.length, 0);

// Unregister multiple targets.
heldValues = [];
let token2 = {};
registry.register({}, 1, token);
registry.register({}, 2, token2);
registry.register({}, 3, token);
registry.register({}, 4, token2);
registry.unregister(token);
gc();
drainJobQueue();
assertEq(heldValues.length, 2);
heldValues = heldValues.sort((a, b) => a - b);
assertEq(heldValues[0], 2);
assertEq(heldValues[1], 4);

// Watch object in another global.
let other = newGlobal({newCompartment: true});
heldValues = [];
registry.register(evalcx('({})', other), 1);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 1);

// Pass heldValues from another global.
let heldValue = evalcx('{}', other);
heldValues = [];
registry.register({}, heldValue);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], heldValue);

// Pass unregister token from another global.
token = evalcx('({})', other);
heldValues = [];
registry.register({}, 1, token);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 1);
heldValues = [];
registry.register({}, 1, token);
registry.unregister(token);
gc();
drainJobQueue();
assertEq(heldValues.length, 0);

// FinalizationRegistry is designed to be subclassable.
class MyRegistry extends FinalizationRegistry {
  constructor(callback) {
    super(callback);
  }
}
let r2 = new MyRegistry(value => {
  heldValues.push(value);
});
heldValues = [];
r2.register({}, 42);
gc();
drainJobQueue();
assertEq(heldValues.length, 1);
assertEq(heldValues[0], 42);

// Test cleanupSome.
heldValues = [];
let r5 = new FinalizationRegistry(v => heldValues.push(v));
r5.register({}, 1);
r5.register({}, 2);
r5.register({}, 3);
gc();
r5.cleanupSome();
assertEq(heldValues.length, 3);
heldValues = heldValues.sort((a, b) => a - b);
assertEq(heldValues[0], 1);
assertEq(heldValues[1], 2);
assertEq(heldValues[2], 3);

// Test trying to call cleanupSome in callback.
let r6 = new FinalizationRegistry(x => {
  r6.cleanupSome();
});
r6.register({}, 1);
gc();
drainJobQueue();

// Test trying to call cleanupSome in callback with multiple values.
let callbackCounter7 = 0;
let r7 = new FinalizationRegistry(x => {
  callbackCounter7++;
  r7.cleanupSome();
});
r7.register({}, 1);
r7.register({}, 2);
r7.register({}, 3);
r7.register({}, 4);
gc();
drainJobQueue();
assertEq(callbackCounter7, 4);

// Test that targets don't keep the finalization registry alive.
let target = {};
registry = new FinalizationRegistry(value => undefined);
registry.register(target, 1);
let weakRef = new WeakRef(registry);
registry = undefined;
assertEq(typeof weakRef.deref(), 'object');
drainJobQueue();
gc();
assertEq(weakRef.deref(), undefined);
assertEq(typeof target, 'object');

// Test that targets don't keep the finalization registry alive when also
// used as the unregister token.
registry = new FinalizationRegistry(value => undefined);
registry.register(target, 1, target);
weakRef = new WeakRef(registry);
registry = undefined;
assertEq(typeof weakRef.deref(), 'object');
drainJobQueue();
gc();
assertEq(weakRef.deref(), undefined);
assertEq(typeof target, 'object');

// Test that cleanup doesn't happen if the finalization registry dies.
heldValues = [];
new FinalizationRegistry(value => {
  heldValues.push(value);
}).register({}, 1);
gc();
drainJobQueue();
assertEq(heldValues.length, 0);
