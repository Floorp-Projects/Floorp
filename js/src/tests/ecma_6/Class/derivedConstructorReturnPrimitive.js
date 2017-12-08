class foo extends null {
    constructor() {
        // Returning a primitive is a TypeError in derived constructors. This
        // ensures that super() can take the return value directly, without
        // checking it. Use |null| here, as a tricky check to make sure we
        // didn't lump it in with the object check, somehow.
        return null;
    }
}

for (let i = 0; i < 1100; i++)
    assertThrownErrorContains(() => new foo(), "return");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
