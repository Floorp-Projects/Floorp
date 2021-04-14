// Test importEntries property

function importEntryEq(a, b)
{
    return a['moduleRequest']['specifier'] === b['moduleRequest']['specifier'] &&
           a['importName'] === b['importName'] &&
           a['localName'] === b['localName'];
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
                  [{moduleRequest: {specifier: 'mod'}, importName: 'default', localName: 'v'}]);

testImportEntries('import * as ns from "mod";',
                  [{moduleRequest: {specifier: 'mod'}, importName: null, localName: 'ns'}]);

testImportEntries('import {x} from "mod";',
                  [{moduleRequest: {specifier: 'mod'}, importName: 'x', localName: 'x'}]);

testImportEntries('import {x as v} from "mod";',
                  [{moduleRequest: {specifier: 'mod'}, importName: 'x', localName: 'v'}]);

testImportEntries('import "mod";',
                  []);

testImportEntries('import {x} from "a"; import {y} from "b";',
                  [{moduleRequest: {specifier: 'a'}, importName: 'x', localName: 'x'},
                   {moduleRequest: {specifier: 'b'}, importName: 'y', localName: 'y'}]);
