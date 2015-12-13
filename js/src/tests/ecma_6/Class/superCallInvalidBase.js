class instance extends null {
    constructor() { super(); }
}

assertThrowsInstanceOf(() => new instance(), TypeError);
assertThrowsInstanceOf(() => new class extends null { }(), TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
