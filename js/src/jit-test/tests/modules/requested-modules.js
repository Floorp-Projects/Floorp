// |jit-test| --enable-import-attributes

// Test requestedModules property

function testRequestedModules(source, expected) {
    var module = parseModule(source);
    var actual = module.requestedModules;
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i].moduleRequest.specifier, expected[i].specifier);
        if(expected[i].attributes === null) {
            assertEq(actual[i].moduleRequest.attributes, null);
        }
        else {
            var expectedAttributes = expected[i].attributes;
            var actualAttributes = actual[i].moduleRequest.attributes;
            assertEq(actualAttributes.length, expectedAttributes.length);
            for (var j = 0; j < expectedAttributes.length; j++) {
                assertEq(expectedAttributes[j].type, actualAttributes[j].type);
            }
        }
    }
}

testRequestedModules("", []);

testRequestedModules("import a from 'foo'", [
    { specifier: 'foo', attributes: null }
]);

testRequestedModules("import a from 'foo'; import b from 'bar'", [
    { specifier: 'foo', attributes: null },
    { specifier: 'bar', attributes: null }
]);

testRequestedModules("import a from 'foo'; import b from 'bar'; import c from 'foo'", [
    { specifier: 'foo', attributes: null },
    { specifier: 'bar', attributes: null }
]);

testRequestedModules("export {} from 'foo'", [
    { specifier: 'foo', attributes: null }
]);

testRequestedModules("export * from 'bar'",[
    { specifier: 'bar', attributes: null }
]);

testRequestedModules("import a from 'foo'; export {} from 'bar'; export * from 'baz'", [
    { specifier: 'foo', attributes: null },
    { specifier: 'bar', attributes: null },
    { specifier: 'baz', attributes: null }
]);

if (getRealmConfiguration("importAttributes")) {
    testRequestedModules("import a from 'foo' with {}", [
        { specifier: 'foo', attributes: null },
    ]);

    testRequestedModules("import a from 'foo' with { type: 'js'}", [
        { specifier: 'foo', attributes: [ { type: 'js' } ] },
    ]);

    testRequestedModules("import a from 'foo' with { unsupported: 'test'}", [
        { specifier: 'foo', attributes: null },
    ]);

    testRequestedModules("import a from 'foo' with { unsupported: 'test', type: 'js', foo: 'bar' }", [
        { specifier: 'foo', attributes: [ { type: 'js' } ] },
    ]);

    testRequestedModules("import a from 'foo' with { type: 'js1'}; export {} from 'bar' with { type: 'js2'}; export * from 'baz' with { type: 'js3'}", [
        { specifier: 'foo', attributes: [ { type: 'js1' } ] },
        { specifier: 'bar', attributes: [ { type: 'js2' } ] },
        { specifier: 'baz', attributes: [ { type: 'js3' } ] }
    ]);

    testRequestedModules("export {} from 'foo' with { type: 'js'}", [
        { specifier: 'foo', attributes:  [ { type: 'js' } ] }
    ]);

    testRequestedModules("export * from 'bar' with { type: 'json'}",[
        { specifier: 'bar', attributes:  [ { type: 'json' } ] }
    ]);

    testRequestedModules("import a from 'foo'; import b from 'bar' with { type: 'json' };", [
        { specifier: 'foo', attributes: null },
        { specifier: 'bar', attributes: [ { type: 'json' } ] },
    ]);

    testRequestedModules("import b from 'bar' with { type: 'json' }; import a from 'foo';", [
        { specifier: 'bar', attributes: [ { type: 'json' } ] },
        { specifier: 'foo', attributes: null },
    ]);
}
