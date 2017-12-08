class testBasic {
    constructor() { }
    static constructor() { }
}

class testWithExtends extends null {
    constructor() { };
    static constructor() { };
}

class testOrder {
    static constructor() { };
    constructor() { };
}

class testOrderWithExtends extends null {
    static constructor() { };
    constructor() { };
}

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
