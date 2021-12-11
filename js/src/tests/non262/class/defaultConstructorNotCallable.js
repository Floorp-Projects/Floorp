class badBase {}
assertThrowsInstanceOf(badBase, TypeError);

class badSub extends (class {}) {}
assertThrowsInstanceOf(badSub, TypeError);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
