// |jit-test| --enable-import-assertions

// Test requestedModules property

function testRequestedModules(source, expected) {
    var module = parseModule(source);
    var actual = module.requestedModules;
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i].moduleRequest.specifier, expected[i].specifier);
        if(expected[i].assertions === null) {
            assertEq(actual[i].moduleRequest.assertions, null);
        }
        else {
            var expectedAssertions = expected[i].assertions;
            var actualAssertions = actual[i].moduleRequest.assertions;
            assertEq(actualAssertions.length, expectedAssertions.length);
            for (var j = 0; j < expectedAssertions.length; j++) {
                assertEq(expectedAssertions[j].type, actualAssertions[j].type);
            }
        }
    }
}

testRequestedModules("", []);

testRequestedModules("import a from 'foo'", [
    { specifier: 'foo', assertions: null }
]);

testRequestedModules("import a from 'foo'; import b from 'bar'", [
    { specifier: 'foo', assertions: null },
    { specifier: 'bar', assertions: null }
]);

testRequestedModules("import a from 'foo'; import b from 'bar'; import c from 'foo'", [
    { specifier: 'foo', assertions: null },
    { specifier: 'bar', assertions: null }
]);

testRequestedModules("export {} from 'foo'", [
    { specifier: 'foo', assertions: null }
]);

testRequestedModules("export * from 'bar'",[
    { specifier: 'bar', assertions: null }
]);

testRequestedModules("import a from 'foo'; export {} from 'bar'; export * from 'baz'", [
    { specifier: 'foo', assertions: null },
    { specifier: 'bar', assertions: null },
    { specifier: 'baz', assertions: null }
]);

if (getRealmConfiguration("importAttributes")) {
    testRequestedModules("import a from 'foo' assert {}", [
        { specifier: 'foo', assertions: null },
    ]);

    testRequestedModules("import a from 'foo' assert { type: 'js'}", [
        { specifier: 'foo', assertions: [ { type: 'js' } ] },
    ]);

    testRequestedModules("import a from 'foo' assert { unsupported: 'test'}", [
        { specifier: 'foo', assertions: null },
    ]);

    testRequestedModules("import a from 'foo' assert { unsupported: 'test', type: 'js', foo: 'bar' }", [
        { specifier: 'foo', assertions: [ { type: 'js' } ] },
    ]);

    testRequestedModules("import a from 'foo' assert { type: 'js1'}; export {} from 'bar' assert { type: 'js2'}; export * from 'baz' assert { type: 'js3'}", [
        { specifier: 'foo', assertions: [ { type: 'js1' } ] },
        { specifier: 'bar', assertions: [ { type: 'js2' } ] },
        { specifier: 'baz', assertions: [ { type: 'js3' } ] }
    ]);

    testRequestedModules("export {} from 'foo' assert { type: 'js'}", [
        { specifier: 'foo', assertions:  [ { type: 'js' } ] }
    ]);

    testRequestedModules("export * from 'bar' assert { type: 'json'}",[
        { specifier: 'bar', assertions:  [ { type: 'json' } ] }
    ]);

    testRequestedModules("import a from 'foo'; import b from 'bar' assert { type: 'json' };", [
        { specifier: 'foo', assertions: null },
        { specifier: 'bar', assertions: [ { type: 'json' } ] },
    ]);

    testRequestedModules("import b from 'bar' assert { type: 'json' }; import a from 'foo';", [
        { specifier: 'bar', assertions: [ { type: 'json' } ] },
        { specifier: 'foo', assertions: null },
    ]);
}
