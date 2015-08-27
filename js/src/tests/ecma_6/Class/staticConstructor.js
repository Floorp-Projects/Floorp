var test = `
class test {
    constructor() { }
    static constructor() { }
}

class testWithExtends {
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
`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0,0,"OK");
