function f(a) {
    return a.toString();
}
var g = newGlobal({sameCompartmentAs: this});
g.evaluate("function Obj() {}");
f(f(new g.Obj()));
