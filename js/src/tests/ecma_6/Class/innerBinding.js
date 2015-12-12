// Named class definitions should create an immutable inner binding.
// Since all code in classes is in strict mode, attempts to mutate it
// should throw.
class Foof { constructor() { }; tryBreak() { Foof = 4; } }
for (let result of [Foof, class Bar { constructor() { }; tryBreak() { Bar = 4; } }])
    assertThrowsInstanceOf(() => new result().tryBreak(), TypeError);

{
    class foo { constructor() { }; tryBreak() { foo = 4; } }
    for (let result of [foo, class Bar { constructor() { }; tryBreak() { Bar = 4 } }])
        assertThrowsInstanceOf(() => new result().tryBreak(), TypeError);
}

// TDZ applies to inner bindings
assertThrowsInstanceOf(()=>eval(`class Bar {
                                    constructor() { };
                                    [Bar] () { };
                                 }`), ReferenceError);

assertThrowsInstanceOf(()=>eval(`(class Bar {
                                    constructor() { };
                                    [Bar] () { };
                                 })`), ReferenceError);

// There's no magic "inner binding" global
{
    class Foo {
        constructor() { };
        test() {
            class Bar {
                constructor() { }
                test() { return Foo === Bar }
            }
            return new Bar().test();
        }
    }
    assertEq(new Foo().test(), false);
    assertEq(new class foo {
        constructor() { };
        test() {
            return new class bar {
                constructor() { }
                test() { return foo === bar }
            }().test();
        }
    }().test(), false);
}

// Inner bindings are shadowable
{
    class Foo {
        constructor() { }
        test(Foo) { return Foo; }
    }
    assertEq(new Foo().test(4), 4);
    assertEq(new class foo {
        constructor() { };
        test(foo) { return foo }
    }().test(4), 4);
}

// Inner bindings in expressions should shadow even existing names.
class Foo { constructor() { } static method() { throw new Error("NO!"); } }
assertEq(new class Foo {
            constructor() { };
            static method() { return 4; };
            test() { return Foo.method(); }
         }().test(), 4);

// The outer binding is distinct from the inner one
{
    let orig_X;

    class X {
        constructor() { }
        f() { assertEq(X, orig_X); }
    }

    orig_X = X;
    X = 13;
    assertEq(X, 13);
    new orig_X().f();
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
