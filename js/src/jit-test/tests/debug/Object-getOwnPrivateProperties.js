// private fields

var g = newGlobal({ newCompartment: true });
var dbg = Debugger();
var gobj = dbg.addDebuggee(g);

g.eval(`
class MyClass {
    constructor() {
        this.publicProperty = 1;
        this.publicSymbol = Symbol("public");
        this[this.publicSymbol] = 2;
        this.#privateProperty1 = 3;
        this.#privateProperty2 = 4;
    }
    static #privateStatic1
    static #privateStatic2
    #privateProperty1
    #privateProperty2
    #privateMethod() {}
    publicMethod(){}
}
obj = new MyClass();
klass = MyClass`);

var privatePropertiesSymbolsDescriptions = gobj
  .getOwnPropertyDescriptor("obj")
  .value.getOwnPrivateProperties()
  .map(sym => sym.description);

assertEq(
  JSON.stringify(privatePropertiesSymbolsDescriptions),
  JSON.stringify([`#privateProperty1`, `#privateProperty2`])
);

var classPrivatePropertiesSymbolsDescriptions = gobj
  .getOwnPropertyDescriptor("klass")
  .value.getOwnPrivateProperties()
  .map(sym => sym.description);

assertEq(
  JSON.stringify(classPrivatePropertiesSymbolsDescriptions),
  JSON.stringify([`#privateStatic1`, `#privateStatic2`])
);
