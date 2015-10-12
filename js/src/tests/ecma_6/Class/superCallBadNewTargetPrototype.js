var test = `

class base { constructor() { } }

// lies and the lying liars who tell them
function lies() { }
lies.prototype = 4;

assertThrowsInstanceOf(()=>Reflect.consruct(base, [], lies), TypeError);

// lie a slightly different way
function get(target, property, receiver) {
    if (property === "prototype")
        return 42;
    return Reflect.get(target, property, receiver);
}

class inst extends base {
    constructor() { super(); }
}
assertThrowsInstanceOf(()=>new new Proxy(inst, {get})(), TypeError);

class defaultInst extends base {}
assertThrowsInstanceOf(()=>new new Proxy(defaultInst, {get})(), TypeError);

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === 'function')
    reportCompare(0,0,"OK");
