// |jit-test| --enable-private-methods;

class C {
    #x() { }
    constructor() { this.#x = 1; }
}

try {
    new C
} catch (e) {
    assertEq(e.message, "#x is read-only")
}
