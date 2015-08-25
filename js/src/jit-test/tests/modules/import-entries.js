// Test importEntries property

function testImportEntries(source, expected) {
    var module = parseModule(source);
    var actual = module.importEntries;
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        for (var property in expected[i]) {
            assertEq(actual[i][property], expected[i][property]);
        }
    }
}

testImportEntries('', []);

testImportEntries('import v from "mod";',
                  [{moduleRequest: 'mod', importName: 'default', localName: 'v'}]);

testImportEntries('import * as ns from "mod";',
                  [{moduleRequest: 'mod', importName: '*', localName: 'ns'}]);

testImportEntries('import {x} from "mod";',
                  [{moduleRequest: 'mod', importName: 'x', localName: 'x'}]);

testImportEntries('import {x as v} from "mod";',
                  [{moduleRequest: 'mod', importName: 'x', localName: 'v'}]);

testImportEntries('import "mod";',
                  []);

testImportEntries('import {x} from "a"; import {y} from "b";',
                  [{moduleRequest: 'a', importName: 'x', localName: 'x'},
                   {moduleRequest: 'b', importName: 'y', localName: 'y'}]);
