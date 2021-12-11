// Test localExportEntries property

function testObjectContents(actual, expected) {
    for (var property in expected) {
        if(actual[property] instanceof Object) {
            testObjectContents(actual[property], expected[property]);
        }
        else {
            assertEq(actual[property], expected[property]);
        }
    }
}

function testArrayContents(actual, expected) {
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        testObjectContents(actual[i], expected[i]);
    }
}

function testLocalExportEntries(source, expected) {
    var module = parseModule(source);
    testArrayContents(module.localExportEntries, expected);
}

testLocalExportEntries(
    'export var v;',
    [{exportName: 'v', moduleRequest: null, importName: null, localName: 'v'}]);

testLocalExportEntries(
    'export var v = 0;',
    [{exportName: 'v', moduleRequest: null, importName: null, localName: 'v'}]);

testLocalExportEntries(
    'export let x = 1;',
    [{exportName: 'x', moduleRequest: null, importName: null, localName: 'x'}]);

testLocalExportEntries(
    'export const x = 1;',
    [{exportName: 'x', moduleRequest: null, importName: null, localName: 'x'}]);

testLocalExportEntries(
    'export class foo { constructor() {} };',
    [{exportName: 'foo', moduleRequest: null, importName: null, localName: 'foo'}]);

testLocalExportEntries(
    'export default function f() {};',
    [{exportName: 'default', moduleRequest: null, importName: null, localName: 'f'}]);

testLocalExportEntries(
    'export default function() {};',
    [{exportName: 'default', moduleRequest: null, importName: null, localName: 'default'}]);

testLocalExportEntries(
    'export default 42;',
    [{exportName: 'default', moduleRequest: null, importName: null, localName: 'default'}]);

testLocalExportEntries(
    'let x = 1; export {x};',
    [{exportName: 'x', moduleRequest: null, importName: null, localName: 'x'}]);

testLocalExportEntries(
    'let v = 1; export {v as x};',
    [{exportName: 'x', moduleRequest: null, importName: null, localName: 'v'}]);

testLocalExportEntries(
    'export {x} from "mod";',
    []);

testLocalExportEntries(
    'export {v as x} from "mod";',
    []);

testLocalExportEntries(
    'export * from "mod";',
    []);

// Test indirectExportEntries property

function testIndirectExportEntries(source, expected) {
    var module = parseModule(source);
    testArrayContents(module.indirectExportEntries, expected);
}

testIndirectExportEntries(
    'export default function f() {};',
    []);

testIndirectExportEntries(
    'let x = 1; export {x};',
    []);

testIndirectExportEntries(
    'export {x} from "mod";',
    [{exportName: 'x', moduleRequest: {specifier:'mod'}, importName: 'x', localName: null}]);

testIndirectExportEntries(
    'export {v as x} from "mod";',
    [{exportName: 'x', moduleRequest: {specifier:'mod'}, importName: 'v', localName: null}]);

testIndirectExportEntries(
    'export * from "mod";',
    []);

testIndirectExportEntries(
    'import {v as x} from "mod"; export {x as y};',
    [{exportName: 'y', moduleRequest: {specifier:'mod'}, importName: 'v', localName: null}]);

// Test starExportEntries property

function testStarExportEntries(source, expected) {
    var module = parseModule(source);
    testArrayContents(module.starExportEntries, expected);
}

testStarExportEntries(
    'export default function f() {};',
    []);

testStarExportEntries(
    'let x = 1; export {x};',
    []);

testStarExportEntries(
    'export {x} from "mod";',
    []);

testStarExportEntries(
    'export * from "mod";',
    [{exportName: null, moduleRequest: {specifier:'mod'}, importName: null, localName: null}]);
