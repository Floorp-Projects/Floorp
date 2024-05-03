// |jit-test| --enable-import-attributes

// Test importEntries property

function attributeEq(actual, expected) {
    var actualAttributes = actual['attributes'];
    var expectedAttributes = expected['attributes'];

    if(actualAttributes === null) {
        return expectedAttributes === actualAttributes
    }

    if(actualAttributes.length !== expectedAttributes.length) {
        return false;
    }

    for (var i = 0; i < expectedAttributes.length; i++) {
        if(expectedAttributes[i].type !== actualAttributes[i].type) {
            return false;
        }
    }

    return true;
}

function importEntryEq(a, b) {
    var r1 = a['moduleRequest']['specifier'] === b['moduleRequest']['specifier'] &&
        a['importName'] === b['importName'] &&
        a['localName'] === b['localName'];

    return r1 && attributeEq(a['moduleRequest'], b['moduleRequest']);
}

function findImportEntry(array, target)
{
    for (let i = 0; i < array.length; i++) {
        if (importEntryEq(array[i], target))
            return i;
    }
    return -1;
}

function testImportEntries(source, expected) {
    var module = parseModule(source);
    var actual = module.importEntries.slice(0);
    assertEq(actual.length, expected.length);
    for (var i = 0; i < expected.length; i++) {
        let index = findImportEntry(actual, expected[i]);
        assertEq(index >= 0, true);
        actual.splice(index, 1);
    }
}

testImportEntries('', []);

testImportEntries('import v from "mod";',
                  [{moduleRequest: {specifier: 'mod', attributes: null}, importName: 'default', localName: 'v'}]);

testImportEntries('import * as ns from "mod";',
                  [{moduleRequest: {specifier: 'mod', attributes: null}, importName: null, localName: 'ns'}]);

testImportEntries('import {x} from "mod";',
                  [{moduleRequest: {specifier: 'mod', attributes: null}, importName: 'x', localName: 'x'}]);

testImportEntries('import {x as v} from "mod";',
                  [{moduleRequest: {specifier: 'mod', attributes: null}, importName: 'x', localName: 'v'}]);

testImportEntries('import "mod";',
                  []);

testImportEntries('import {x} from "a"; import {y} from "b";',
                  [{moduleRequest: {specifier: 'a', attributes: null}, importName: 'x', localName: 'x'},
                   {moduleRequest: {specifier: 'b', attributes: null}, importName: 'y', localName: 'y'}]);

if (getRealmConfiguration("importAttributes")) {
    testImportEntries('import v from "mod" with {};',
                  [{moduleRequest: {specifier: 'mod', attributes: null}, importName: 'default', localName: 'v'}]);

    testImportEntries('import v from "mod" with { type: "js"};',
        [{moduleRequest: {specifier: 'mod', attributes: [{ type: 'js'}]}, importName: 'default', localName: 'v'}]);

    testImportEntries('import {x} from "mod" with { type: "js"};',
                  [{moduleRequest: {specifier: 'mod', attributes: [{ type: 'js'}]}, importName: 'x', localName: 'x'}]);

    testImportEntries('import {x as v} from "mod" with { type: "js"};',
                  [{moduleRequest: {specifier: 'mod', attributes: [{ type: 'js'}]}, importName: 'x', localName: 'v'}]);
}
