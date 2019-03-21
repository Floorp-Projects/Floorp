class foo extends null {
    constructor() {
        // Explicit returns of undefined should act the same as falling off the
        // end of the function. That is to say, they should throw.
        return undefined;
    }
}

for (let i = 0; i < 1100; i++)
    assertThrownErrorContains(() => new foo(), "this");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
