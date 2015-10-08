// Exercise ModuleDeclarationInstantiation() operation.

function testModuleEnvironment(module, expected) {
    var actual = getModuleEnvironmentNames(module);
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i], expected[i]);
    }
}

// Check the environment of an empty module.
let m = parseModule("");
m.declarationInstantiation();
testModuleEnvironment(m, []);

let moduleRepo = new Map();
setModuleResolveHook(function(module, specifier) {
    if (specifier in moduleRepo)
        return moduleRepo[specifier];
    throw "Module " + specifier + " not found";
});

let a = moduleRepo['a'] = parseModule("var x = 1; export { x };");
let b = moduleRepo['b'] = parseModule("import { x as y } from 'a';");

a.declarationInstantiation();
b.declarationInstantiation();

testModuleEnvironment(a, ['x']);
testModuleEnvironment(b, ['y']);
