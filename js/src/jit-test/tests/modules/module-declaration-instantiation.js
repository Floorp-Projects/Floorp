// Exercise ModuleDeclarationInstantiation() operation.

function parseAndInstantiate(source) {
    let m = parseModule(source);
    m.declarationInstantiation();
    return m.environment;
}

function testModuleEnvironment(module, expected) {
    var actual = Object.keys(module.environment);
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i], expected[i]);
    }
}

// Check the environment of an empty module.
let e = parseAndInstantiate("");
assertEq(Object.keys(e).length, 0);

let moduleRepo = new Map();
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module " + specifier + " not found";
});

a = moduleRepo['a'] = parseModule("var x = 1; export { x };");
b = moduleRepo['b'] = parseModule("import { x as y } from 'a';");

a.declarationInstantiation();
b.declarationInstantiation();

testModuleEnvironment(a, ['x']);
testModuleEnvironment(b, ['y']);
