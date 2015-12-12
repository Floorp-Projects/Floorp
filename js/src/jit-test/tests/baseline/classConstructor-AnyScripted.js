function test(fun) {
    fun();
}

// Generate a CallAnyScripted stub in test()
for (let i = 0; i < 20; i++) {
    test(function() { /* wheeee */ });
}

class foo {
    constructor() { }
}

// Compile foo()
for (let i = 0; i < 11; i++)
    new foo();

try {
    test(foo);
    throw new Error("Invoking a class constructor without new must throw");
} catch (e if e instanceof TypeError) { }
