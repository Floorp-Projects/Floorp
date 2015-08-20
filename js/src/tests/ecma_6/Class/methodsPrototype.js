var test = `
class TestClass {
    constructor() { }
    method() { }
    get getter() { }
    set setter(x) { }
    *generator() { }
    static staticMethod() { }
    static get staticGetter() { }
    static set staticSetter(x) { }
    static *staticGenerator() { }
}

var test = new TestClass();

assertEq(test.constructor.hasOwnProperty('prototype'), true);

var hasNoPrototype = [
    test.method,
    test.generator,
    TestClass.staticGenerator,
    Object.getOwnPropertyDescriptor(test.__proto__, 'getter').get,
    Object.getOwnPropertyDescriptor(test.__proto__, 'setter').set,
    TestClass.staticMethod,
    Object.getOwnPropertyDescriptor(TestClass, 'staticGetter').get,
    Object.getOwnPropertyDescriptor(TestClass, 'staticSetter').set
]

for (var fun of hasNoPrototype) {
    assertEq(fun.hasOwnProperty('prototype'), false);
}

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
