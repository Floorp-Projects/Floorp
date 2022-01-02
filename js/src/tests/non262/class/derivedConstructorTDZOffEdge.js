class foo extends null {
    constructor() {
        // Let it fall off the edge and throw.
    }
}

for (let i = 0; i < 1100; i++)
    assertThrownErrorContains(() => new foo(), "this");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
