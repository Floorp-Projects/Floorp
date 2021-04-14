// Test requestedModules property

function testRequestedModules(source, expected) {
    var module = parseModule(source);
    var actual = module.requestedModules;
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i].moduleRequest.specifier, expected[i]);
    }
}

testRequestedModules("", []);

testRequestedModules("import a from 'foo'",
                     ['foo']);

testRequestedModules("import a from 'foo'; import b from 'bar'",
                     ['foo', 'bar']);

testRequestedModules("import a from 'foo'; import b from 'bar'; import c from 'foo'",
                     ['foo', 'bar']);

testRequestedModules("export {} from 'foo'",
                     ['foo']);

testRequestedModules("export * from 'bar'",
                     ['bar']);

testRequestedModules("import a from 'foo'; export {} from 'bar'; export * from 'baz'",
                     ['foo', 'bar', 'baz']);
