function pleaseRunMyCode() { }

class foo extends null {
    constructor() {
        // Just bareword |this| is DCEd by the BytecodeEmitter. Your guess as
        // to why we think this is a good idea is as good as mine. In order to
        // combat this inanity, make it a function arg, so we have to compute
        // it.
        pleaseRunMyCode(this);
        assertEq(false, true);
    }
}

for (let i = 0; i < 1100; i++)
    assertThrownErrorContains(() => new foo(), "|this|");

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
