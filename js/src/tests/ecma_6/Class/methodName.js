class TestClass {
    get getter() { }
    set setter(x) { }
    method() { }

    static get staticGetter() { }
    static set staticSetter(x) { }
    static staticMethod() { }

    get 1() { }
    set 2(v) { }
    3() { }

    static get 4() { }
    static set 5(x) { }
    static 6() { }
}

function name(obj, property, get) {
    let desc = Object.getOwnPropertyDescriptor(obj, property);
    return (get ? desc.get : desc.set).name;
}

assertEq(name(TestClass.prototype, "getter", true), "get getter");
assertEq(name(TestClass.prototype, "setter", false), "set setter");
assertEq(TestClass.prototype.method.name, "method");

assertEq(name(TestClass, "staticGetter", true), "get staticGetter");
assertEq(name(TestClass, "staticSetter", false), "set staticSetter");
assertEq(TestClass.staticMethod.name, "staticMethod");

assertEq(name(TestClass.prototype, "1", true), "get 1");
assertEq(name(TestClass.prototype, "2", false), "set 2");
assertEq(TestClass.prototype[3].name, "3");

assertEq(name(TestClass, "4", true), "get 4");
assertEq(name(TestClass, "5", false), "set 5");
assertEq(TestClass[6].name, "6");

reportCompare(true, true);
