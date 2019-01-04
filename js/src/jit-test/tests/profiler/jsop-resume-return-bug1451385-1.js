// |jit-test| error:ReferenceError

enableGeckoProfiling("Math.round", "-0.49", -0);
function* g(n) {
    for (var i = 0; i < n; i++) yield i;
}
var inner = g(20);
for (let target of inner) {
    if (GeneratorObjectPrototype() == i(true, true) == (this) == (this)) {}
}
