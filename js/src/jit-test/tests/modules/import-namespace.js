// Test importing module namespaces

"use strict";

load(libdir + "asserts.js");
load(libdir + "iteration.js");
load(libdir + "dummyModuleResolveHook.js");

function parseAndEvaluate(source) {
    let m = parseModule(source);
    m.declarationInstantiation();
    return m.evaluation();
}

function testHasNames(names, expected) {
    assertEq(names.length, expected.length);
    expected.forEach(function(name) {
        assertEq(names.includes(name), true);
    });
}

let a = moduleRepo['a'] = parseModule(
    `export var a = 1;
     export var b = 2;`
);

let b = moduleRepo['b'] = parseModule(
    `import * as ns from 'a';
     export { ns };
     export var x = ns.a + ns.b;`
);

b.declarationInstantiation();
b.evaluation();
testHasNames(getModuleEnvironmentNames(b), ["ns", "x"]);
let ns = getModuleEnvironmentValue(b, "ns");
testHasNames(Object.keys(ns), ["a", "b"]);
assertEq(getModuleEnvironmentValue(b, "x"), 3);

// Test module namespace internal methods as defined in 9.4.6
assertEq(Object.getPrototypeOf(ns), null);
assertThrowsInstanceOf(() => Object.setPrototypeOf(ns, null), TypeError);
assertThrowsInstanceOf(function() { ns.foo = 1; }, TypeError);
assertEq(Object.isExtensible(ns), false);
Object.preventExtensions(ns);
let desc = Object.getOwnPropertyDescriptor(ns, "a");
assertEq(desc.value, 1);
assertEq(desc.writable, true);
assertEq(desc.enumerable, true);
assertEq(desc.configurable, false);
assertEq(typeof desc.get, "undefined");
assertEq(typeof desc.set, "undefined");
assertThrowsInstanceOf(function() { ns.a = 1; }, TypeError);
delete ns.foo;
assertThrowsInstanceOf(function() { delete ns.a; }, TypeError);

// Test @@iterator method.
let iteratorFun = ns[Symbol.iterator];
assertEq(iteratorFun.name, "[Symbol.iterator]");

let iterator = ns[Symbol.iterator]();
assertEq(iterator[Symbol.iterator](), iterator);
assertIteratorNext(iterator, "a");
assertIteratorNext(iterator, "b");
assertIteratorDone(iterator);

// The iterator's next method can only be called on the object it was originally
// associated with.
iterator = ns[Symbol.iterator]();
let iterator2 = ns[Symbol.iterator]();
assertThrowsInstanceOf(() => iterator.next.call({}), TypeError);
assertThrowsInstanceOf(() => iterator.next.call(iterator2), TypeError);
assertEq(iterator.next.call(iterator).value, "a");
assertEq(iterator2.next.call(iterator2).value, "a");

// Test cyclic namespace import and access in module evaluation.
let c = moduleRepo['c'] =
    parseModule("export let c = 1; import * as ns from 'd'; let d = ns.d;");
let d = moduleRepo['d'] =
    parseModule("export let d = 2; import * as ns from 'c'; let c = ns.c;");
c.declarationInstantiation();
d.declarationInstantiation();
assertThrowsInstanceOf(() => c.evaluation(), ReferenceError);

// Test cyclic namespace import.
let e = moduleRepo['e'] =
    parseModule("export let e = 1; import * as ns from 'f'; export function f() { return ns.f }");
let f = moduleRepo['f'] =
    parseModule("export let f = 2; import * as ns from 'e'; export function e() { return ns.e }");
e.declarationInstantiation();
f.declarationInstantiation();
e.evaluation();
f.evaluation();
assertEq(e.namespace.f(), 2);
assertEq(f.namespace.e(), 1);
