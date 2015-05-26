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

var hasPrototype = [
    test.constructor,
    test.generator,
    TestClass.staticGenerator
]

for (var fun of hasPrototype) {
    assertEq(fun.hasOwnProperty('prototype'), true);
}

var hasNoPrototype = [
    test.method,
    Object.getOwnPropertyDescriptor(test.__proto__, 'getter').get,
    Object.getOwnPropertyDescriptor(test.__proto__, 'setter').set,
    TestClass.staticMethod,
    Object.getOwnPropertyDescriptor(TestClass, 'staticGetter').get,
    Object.getOwnPropertyDescriptor(TestClass, 'staticSetter').set,
]

for (var fun of hasNoPrototype) {
    assertEq(fun.hasOwnProperty('prototype'), false);
}

`;

if (classesEnabled())
    eval(test);

if (typeof reportCompare === "function")
    reportCompare(0, 0, "OK");
