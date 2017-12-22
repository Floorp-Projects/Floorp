class foo extends null {
    constructor() {
        // If you return an object, we don't care that |this| went
        // uninitialized
        return {};
    }
}

for (let i = 0; i < 1100; i++)
    new foo();

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
