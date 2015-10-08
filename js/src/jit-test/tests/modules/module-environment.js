load(libdir + "class.js");

// Test top-level module environment

function testInitialEnvironment(source, expected) {
    let module = parseModule(source);
    let names = getModuleEnvironmentNames(module);
    assertEq(names.length, expected.length);
    expected.forEach(function(name) {
        assertEq(names.includes(name), true);
    });
}

testInitialEnvironment('', []);
testInitialEnvironment('var x = 1;', ['x']);
testInitialEnvironment('let x = 1;', ['x']);
testInitialEnvironment("if (true) { let x = 1; }", []);
testInitialEnvironment("if (true) { var x = 1; }", ['x']);
testInitialEnvironment('function x() {}', ['x']);
testInitialEnvironment('export var x = 1;', ['x']);
testInitialEnvironment('export let x = 1;', ['x']);
testInitialEnvironment('export default function x() {};', ['x']);
testInitialEnvironment('export default 1;', ['*default*']);
testInitialEnvironment('export default function() {};', ['*default*']);

if (classesEnabled()) {
    testInitialEnvironment("class x { constructor() {} }", ['x']);
    testInitialEnvironment('export default class x { constructor() {} };', ['x']);
    testInitialEnvironment('export default class { constructor() {} };', ['*default*']);
}
