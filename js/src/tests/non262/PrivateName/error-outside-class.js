
// Can't reference a private field without an object
assertThrowsInstanceOf(() => eval('#x'), SyntaxError);

// Can't reference a private field without an enclosing class
assertThrowsInstanceOf(() => eval('this.#x'), SyntaxError);

// Can't reference a private field in a random function outside a class context
assertThrowsInstanceOf(
    () => eval('function foo() { return this.#x'), SyntaxError);


if (typeof reportCompare === 'function') reportCompare(0, 0);
