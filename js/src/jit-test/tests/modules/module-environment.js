// Test top-level module environment

function testInitialEnvironment(source, expected) {
    let module = parseModule(source);
    let names = getModuleEnvironmentNames(module);
    assertEq(names.length, expected.length);
    expected.forEach(function(name) {
        assertEq(names.includes(name), true);
    });
}

// Non-exported bindings: only top-level functions are present in the
// environment.
testInitialEnvironment('', []);
testInitialEnvironment('var x = 1;', []);
testInitialEnvironment('let x = 1;', []);
testInitialEnvironment("if (true) { let x = 1; }", []);
testInitialEnvironment("if (true) { var x = 1; }", []);
testInitialEnvironment('function x() {}', ['x']);
testInitialEnvironment("class x { constructor() {} }", []);

// Exported bindings must be present in the environment.
testInitialEnvironment('export var x = 1;', ['x']);
testInitialEnvironment('export let x = 1;', ['x']);
testInitialEnvironment('export default function x() {};', ['x']);
testInitialEnvironment('export default 1;', ['*default*']);
testInitialEnvironment('export default function() {};', ['*default*']);
testInitialEnvironment("export class x { constructor() {} }", ['x']);
testInitialEnvironment('export default class x { constructor() {} };', ['x']);
testInitialEnvironment('export default class { constructor() {} };', ['*default*']);

// Imports: namespace imports are present.
testInitialEnvironment('import { x } from "m";', []);
testInitialEnvironment('import * as x from "m";', ['x']);
