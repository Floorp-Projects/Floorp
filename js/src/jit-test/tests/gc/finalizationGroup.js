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
new FinalizationGroup(it => iterator = it).register({}, 0);
gc();
drainJobQueue();
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

let holdings = [];
group = new FinalizationGroup(iterator => {
    for (const holding of iterator) {
        holdings.push(holding);
    }
});

// Test a single target.
holdings = [];
group.register({}, 42);
gc();
drainJobQueue();
assertEq(holdings.length, 1);
assertEq(holdings[0], 42);

// Test multiple targets.
holdings = [];
for (let i = 0; i < 100; i++) {
    group.register({}, i);
}
gc();
drainJobQueue();
assertEq(holdings.length, 100);
holdings = holdings.sort((a, b) => a - b);
for (let i = 0; i < 100; i++) {
    assertEq(holdings[i], i);
}

// Test a single object in multiple groups
holdings = [];
let holdings2 = [];
let group2 = new FinalizationGroup(iterator => {
    for (const holding of iterator) {
        holdings2.push(holding);
    }
});
{
    let object = {};
    group.register(object, 1);
    group2.register(object, 2);
    object = null;
}
gc();
drainJobQueue();
assertEq(holdings.length, 1);
assertEq(holdings[0], 1);
assertEq(holdings2.length, 1);
assertEq(holdings2[0], 2);

// Unregister a single target.
holdings = [];
let token = {};
group.register({}, 1, token);
group.unregister(token);
gc();
drainJobQueue();
assertEq(holdings.length, 0);

// Unregister multiple targets.
holdings = [];
let token2 = {};
group.register({}, 1, token);
group.register({}, 2, token2);
group.register({}, 3, token);
group.register({}, 4, token2);
group.unregister(token);
gc();
drainJobQueue();
assertEq(holdings.length, 2);
holdings = holdings.sort((a, b) => a - b);
assertEq(holdings[0], 2);
assertEq(holdings[1], 4);

// OOM test.
if ('oomTest' in this) {
    oomTest(() => new FinalizationGroup(x => 0));

    let token = {};
    oomTest(() => group.register({}, 1, token));
    oomTest(() => group.unregister(token));
}

// Watch object in another global.
let other = newGlobal({newCompartment: true});
holdings = [];
group.register(evalcx('({})', other), 1);
gc();
drainJobQueue();
assertEq(holdings.length, 1);
assertEq(holdings[0], 1);

// Pass holdings from another global.
let holding = evalcx('{}', other);
holdings = [];
group.register({}, holding);
gc();
drainJobQueue();
assertEq(holdings.length, 1);
assertEq(holdings[0], holding);

// Pass unregister token from another global.
token = evalcx('({})', other);
holdings = [];
group.register({}, 1, token);
gc();
drainJobQueue();
assertEq(holdings.length, 1);
assertEq(holdings[0], 1);
holdings = [];
group.register({}, 1, token);
group.unregister(token);
gc();
drainJobQueue();
assertEq(holdings.length, 0);

// FinalizationGroup is designed to be subclassable.
class MyGroup extends FinalizationGroup {
    constructor(callback) {
        super(callback);
    }
}
let g2 = new MyGroup(iterator => {
    for (const holding of iterator) {
        holdings.push(holding);
    }
});
holdings = [];
g2.register({}, 42);
gc();
drainJobQueue();
assertEq(holdings.length, 1);
assertEq(holdings[0], 42);

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
holdings = [];
let g5 = new FinalizationGroup(i => holdings = [...i]);
g5.register({}, 1);
g5.register({}, 2);
g5.register({}, 3);
gc();
g5.cleanupSome();
assertEq(holdings.length, 3);
holdings = holdings.sort((a, b) => a - b);
assertEq(holdings[0], 1);
assertEq(holdings[1], 2);
assertEq(holdings[2], 3);

// Test trying to call cleanupSome in callback.
let g6 = new FinalizationGroup(x => {
    assertThrowsTypeError(() => g6.cleanupSome());
});
g6.register({}, 1);
gc();
drainJobQueue();

// Test combinations of arguments in different compartments.
function ccwToObject() {
    return evalcx('({})', newGlobal({newCompartment: true}));
}
function ccwToGroup() {
    let global = newGlobal({newCompartment: true});
    global.holdings = holdings;
    return global.eval(`
      new FinalizationGroup(iterator => {
        for (const holding of iterator) {
          holdings.push(holding);
        }
      })`);
}
function incrementalGC() {
    startgc(1);
    while (gcstate() !== "NotActive") {
        gcslice(1000);
    }
}
for (let w of [false, true]) {
    for (let x of [false, true]) {
        for (let y of [false, true]) {
            for (let z of [false, true]) {
                let g = w ? ccwToGroup(w) : group;
                let target = x ? ccwToObject() : {};
                let holding = y ? ccwToObject() : {};
                let token = z ? ccwToObject() : {};
                g.register(target, holding, token);
                g.unregister(token);
                g.register(target, holding, token);
                target = undefined;
                incrementalGC();
                holdings.length = 0; // Clear, don't replace.
                g.cleanupSome();
                assertEq(holdings.length, 1);
                assertEq(holdings[0], holding);
            }
        }
    }
}
