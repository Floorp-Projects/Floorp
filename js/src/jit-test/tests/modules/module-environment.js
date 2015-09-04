// Test top-level module environment

function testInitialEnvironment(source, expected) {
    print(source);
    let m = parseModule(source);
    let scope = m.initialEnvironment;
    let keys = Object.keys(scope);
    assertEq(keys.length, expected.length);
    expected.forEach(function(name) {
        assertEq(name in scope, true);
    });
}

testInitialEnvironment('', []);
testInitialEnvironment('var x = 1;', ['x']);
testInitialEnvironment('let x = 1;', ['x']);
testInitialEnvironment("if (true) { let x = 1; }", []);
testInitialEnvironment("if (true) { var x = 1; }", ['x']);
testInitialEnvironment('function x() {}', ['x']);
testInitialEnvironment("class x { constructor() {} }", ['x']);
testInitialEnvironment('export var x = 1;', ['x']);
testInitialEnvironment('export let x = 1;', ['x']);
testInitialEnvironment('export default class x { constructor() {} };', ['x']);
testInitialEnvironment('export default function x() {};', ['x']);
testInitialEnvironment('export default 1;', ['*default*']);
testInitialEnvironment('export default class { constructor() {} };', ['*default*']);
testInitialEnvironment('export default function() {};', ['*default*']);
