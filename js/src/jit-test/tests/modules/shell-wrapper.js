// |jit-test| module
// Test shell ModuleObject wrapper's accessors and methods

load(libdir + "asserts.js");

function testGetter(obj, name) {
  // Check the getter is defined on the instance, instead of prototype.
  //   * raw ModuleObject's getters are defined on prototype
  //   * ModuleObject wrapper's getters are defined on instance
  const desc = Object.getOwnPropertyDescriptor(obj, name);
  assertEq(typeof desc.get, "function");
  assertEq(Object.getOwnPropertyDescriptor(Object.getPrototypeOf(obj), name),
           undefined);

  // Check invalid this value.
  assertThrowsInstanceOf(() => {
    desc.get.call({});
  }, Error);
}

// ==== namespace getter ====
const a = registerModule('a', parseModule(`
export const v = 10;
`));
const b = registerModule('b', parseModule(`
import * as ns from 'a'
`));
moduleLink(b);
moduleEvaluate(b);
assertEq(a.namespace.v, 10);
testGetter(a, "namespace");

// ==== status getter ====
const MODULE_STATUS_UNLINKED = 0;
const MODULE_STATUS_LINKED = 2;
const MODULE_STATUS_EVALUATED = 5;

const c = registerModule('c', parseModule(`
`));
assertEq(c.status, MODULE_STATUS_UNLINKED);
moduleLink(c);
assertEq(c.status, MODULE_STATUS_LINKED);
moduleEvaluate(c);
assertEq(c.status, MODULE_STATUS_EVALUATED);
testGetter(c, "status");

// ==== evaluationError getter ====
const d = registerModule('d', parseModule(`
f();
`));
moduleLink(d);
try {
  await moduleEvaluate(d);
} catch (e) {
}
assertEq(d.evaluationError instanceof ReferenceError, true);
testGetter(d, "evaluationError");

// ==== requestedModules getter ====
const e = parseModule(`
import a from 'b';
`);
assertEq(e.requestedModules.length, 1);
assertEq(e.requestedModules[0].moduleRequest.specifier, 'b');
assertEq(e.requestedModules[0].lineNumber, 2);
assertEq(e.requestedModules[0].columnNumber, 15);
testGetter(e, "requestedModules");
testGetter(e.requestedModules[0], "moduleRequest");
testGetter(e.requestedModules[0].moduleRequest, "specifier");
testGetter(e.requestedModules[0], "lineNumber");
testGetter(e.requestedModules[0], "columnNumber");

// ==== importEntries getter ====
const f = parseModule(`
import {a as A} from 'b';
`);
assertEq(f.importEntries.length, 1);
assertEq(f.importEntries[0].moduleRequest.specifier, 'b');
assertEq(f.importEntries[0].importName, 'a');
assertEq(f.importEntries[0].localName, 'A');
assertEq(f.importEntries[0].lineNumber, 2);
assertEq(f.importEntries[0].columnNumber, 9);
testGetter(f, "importEntries");
testGetter(f.importEntries[0], "moduleRequest");
testGetter(f.importEntries[0].moduleRequest, "specifier");
testGetter(f.importEntries[0], "importName");
testGetter(f.importEntries[0], "localName");
testGetter(f.importEntries[0], "lineNumber");
testGetter(f.importEntries[0], "columnNumber");

// ==== localExportEntries getter ====
const g = parseModule(`
export const v = 1;
`);
assertEq(g.localExportEntries.length, 1);
assertEq(g.localExportEntries[0].exportName, 'v');
assertEq(g.localExportEntries[0].moduleRequest, null);
assertEq(g.localExportEntries[0].importName, null);
assertEq(g.localExportEntries[0].localName, 'v');
assertEq(g.localExportEntries[0].lineNumber, 0);
assertEq(g.localExportEntries[0].columnNumber, 1);
testGetter(g, "localExportEntries");
testGetter(g.localExportEntries[0], "exportName");
testGetter(g.localExportEntries[0], "moduleRequest");
testGetter(g.localExportEntries[0], "importName");
testGetter(g.localExportEntries[0], "localName");
testGetter(g.localExportEntries[0], "lineNumber");
testGetter(g.localExportEntries[0], "columnNumber");

// ==== indirectExportEntries getter ====
const h = parseModule(`
export {v} from "b";
`);
assertEq(h.indirectExportEntries.length, 1);
assertEq(h.indirectExportEntries[0].exportName, 'v');
assertEq(h.indirectExportEntries[0].moduleRequest.specifier, "b");
assertEq(h.indirectExportEntries[0].importName, "v");
assertEq(h.indirectExportEntries[0].localName, null);
assertEq(h.indirectExportEntries[0].lineNumber, 2);
assertEq(h.indirectExportEntries[0].columnNumber, 9);

// ==== starExportEntries getter ====
const i = parseModule(`
export * from "b";
`);
assertEq(i.starExportEntries.length, 1);
assertEq(i.starExportEntries[0].exportName, null);
assertEq(i.starExportEntries[0].moduleRequest.specifier, "b");
assertEq(i.starExportEntries[0].importName, null);
assertEq(i.starExportEntries[0].localName, null);
assertEq(i.starExportEntries[0].lineNumber, 2);
assertEq(i.starExportEntries[0].columnNumber, 8);

// ==== dfsIndex and dfsAncestorIndex getters ====
const j = registerModule('j', parseModule(`
export const v1 = 10;
import {v2} from 'k'
`));
const k = registerModule('k', parseModule(`
export const v2 = 10;
import {v1} from 'j'
`));
const l = registerModule('l', parseModule(`
export const v3 = 10;
import {v2} from 'k'
import {v1} from 'j'
`));
assertEq(j.dfsIndex, undefined);
assertEq(j.dfsAncestorIndex, undefined);
assertEq(k.dfsIndex, undefined);
assertEq(k.dfsAncestorIndex, undefined);
assertEq(l.dfsIndex, undefined);
assertEq(l.dfsAncestorIndex, undefined);
moduleLink(l);
assertEq(j.dfsIndex, 2);
assertEq(j.dfsAncestorIndex, 1);
assertEq(k.dfsIndex, 1);
assertEq(k.dfsAncestorIndex, 1);
assertEq(l.dfsIndex, 0);
assertEq(l.dfsAncestorIndex, 0);

// ==== async and promises getters ====
const m = parseModule(`
`);
assertEq(m.hasTopLevelAwait, false);
assertEq(m.topLevelCapability, undefined);
assertEq(m.asyncEvaluatingPostOrder, undefined);
assertEq(m.asyncParentModules[0], undefined);
assertEq(m.pendingAsyncDependencies, undefined);
testGetter(m, "hasTopLevelAwait");
testGetter(m, "topLevelCapability");
testGetter(m, "asyncEvaluatingPostOrder");
testGetter(m, "asyncParentModules");
testGetter(m, "pendingAsyncDependencies");
