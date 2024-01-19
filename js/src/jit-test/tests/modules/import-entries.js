// |jit-test| --enable-import-assertions

// Test importEntries property

function assertionEq(actual, expected) {
    var actualAssertions = actual['assertions'];
    var expectedAssertions = expected['assertions'];

    if(actualAssertions === null) {
        return expectedAssertions === actualAssertions
    }

    if(actualAssertions.length !== expectedAssertions.length) {
        return false;
    }

    for (var i = 0; i < expected.length; i++) {
        if(expected[i].type !== actual[i].type) {
            return false;
        }
    }

    return true;
}

function importEntryEq(a, b) {
    var r1 = a['moduleRequest']['specifier'] === b['moduleRequest']['specifier'] &&
        a['importName'] === b['importName'] &&
        a['localName'] === b['localName'];

    return r1 && assertionEq(a['moduleRequest'], b['moduleRequest']);
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
                  [{moduleRequest: {specifier: 'mod', assertions: null}, importName: 'default', localName: 'v'}]);

testImportEntries('import * as ns from "mod";',
                  [{moduleRequest: {specifier: 'mod', assertions: null}, importName: null, localName: 'ns'}]);

testImportEntries('import {x} from "mod";',
                  [{moduleRequest: {specifier: 'mod', assertions: null}, importName: 'x', localName: 'x'}]);

testImportEntries('import {x as v} from "mod";',
                  [{moduleRequest: {specifier: 'mod', assertions: null}, importName: 'x', localName: 'v'}]);

testImportEntries('import "mod";',
                  []);

testImportEntries('import {x} from "a"; import {y} from "b";',
                  [{moduleRequest: {specifier: 'a', assertions: null}, importName: 'x', localName: 'x'},
                   {moduleRequest: {specifier: 'b', assertions: null}, importName: 'y', localName: 'y'}]);

if (getRealmConfiguration("importAttributes")) {
    testImportEntries('import v from "mod" assert {};',
                  [{moduleRequest: {specifier: 'mod', assertions: null}, importName: 'default', localName: 'v'}]);

    testImportEntries('import v from "mod" assert { type: "js"};',
        [{moduleRequest: {specifier: 'mod', assertions: [{ type: 'js'}]}, importName: 'default', localName: 'v'}]);

    testImportEntries('import {x} from "mod" assert { type: "js"};',
                  [{moduleRequest: {specifier: 'mod', assertions: [{ type: 'js'}]}, importName: 'x', localName: 'x'}]);

    testImportEntries('import {x as v} from "mod" assert { type: "js"};',
                  [{moduleRequest: {specifier: 'mod', assertions: [{ type: 'js'}]}, importName: 'x', localName: 'v'}]);
}
