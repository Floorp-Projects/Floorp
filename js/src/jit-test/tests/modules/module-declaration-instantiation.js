// Exercise ModuleDeclarationInstantiation() operation.

load(libdir + "dummyModuleResolveHook.js");

function testModuleEnvironment(module, expected) {
    var actual = getModuleEnvironmentNames(module).sort();
    assertEq(actual.length, expected.length);
    for (var i = 0; i < actual.length; i++) {
        assertEq(actual[i], expected[i]);
    }
}

// Check the environment of an empty module.
let m = parseModule("");
m.declarationInstantiation();
testModuleEnvironment(m, []);

let a = moduleRepo['a'] = parseModule("var x = 1; export { x };");
let b = moduleRepo['b'] = parseModule("import { x as y } from 'a';");

a.declarationInstantiation();
b.declarationInstantiation();

testModuleEnvironment(a, ['x']);
testModuleEnvironment(b, ['y']);

// Function bindings are initialized as well as instantiated.
let c = parseModule(`function a(x) { return x; }
                     function b(x) { return x + 1; }
                     function c(x) { return x + 2; }
                     function d(x) { return x + 3; }`);
const names = ['a', 'b', 'c', 'd'];
testModuleEnvironment(c, names);
names.forEach((n) => assertEq(typeof getModuleEnvironmentValue(c, n), "undefined"));
c.declarationInstantiation();
for (let i = 0; i < names.length; i++) {
    let f = getModuleEnvironmentValue(c, names[i]);
    assertEq(f(21), 21 + i);
}
