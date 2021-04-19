// |jit-test| --enable-private-methods;
//
// Bug 1703782 - Assertion failure: this->is<T>(), at vm/JSObject.h:467
var g7 = newGlobal({ newCompartment: true });
g7.parent = this;
g7.eval(`
  Debugger(parent).onEnterFrame = function(frame) {
    let v = frame.environment.getVariable('var0');
  };
`);
class C144252 {
    static #x;
}
