class foo extends null {
    constructor() {
        this;
        assertEq(false, true);
    }
}

for (let i = 0; i < 1100; i++)
    assertThrownErrorContains(() => new foo(), "|this|");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
