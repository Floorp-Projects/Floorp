load(libdir + 'bytecode-cache.js');

// Bury the class definition deep in a lazy function to hit edge cases of lazy
// script handling.

test = `
    function f1() {
        function f2() {
            function f3() {
                class C {
                    constructor() {
                        this.x = 42;
                    }
                }
                return new C;
            }
            return f3();
        }
        return f2();
    }
    if (generation >= 2) {
        assertEq(f1().x, 42);
    }
`;
evalWithCache(test, {});

// NOTE: Fields currently force full parsing, but this test may be more
//       interesting in future.
test = `
    function f1() {
        function f2() {
            function f3() {
                class C {
                    y = 12;

                    constructor() {
                        this.x = 42;
                    }
                }
                return new C;
            }
            return f3();
        }
        return f2();
    }
    if (generation >= 2) {
        assertEq(f1().x, 42);
        assertEq(f1().y, 12);
    }
`;
evalWithCache(test, {});
