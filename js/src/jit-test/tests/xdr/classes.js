load(libdir + 'bytecode-cache.js');

// Prevent relazification triggered by some zeal modes. Relazification can cause
// test failures because lazy functions are XDR'd differently.
gczeal(0);

var test = "new class extends class { } { constructor() { super(); } }()";
evalWithCache(test, { assertEqBytecode : true });

var test = "new class { method() { super.toString(); } }().method()";
evalWithCache(test, { assertEqBytecode : true });

// Test class constructor in lazy function
var test = `
    function f(delazify) {
        function inner1() {
            class Y {
                constructor() {}
            }
        }

        function inner2() {
            class Y {
                constructor() {}
                field1 = "";
            }
        }

        if (delazify) {
            inner1();
            inner2();
        }
    }
    f(generation > 0);
`;
evalWithCache(test, {});
