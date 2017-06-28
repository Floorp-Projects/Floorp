// Test GETPROP_SUPER with megamorphic variation
const NCLASS = 20;

var g_prop = "prop";
var g_THIS = "THIS";

// Define array of base classes with a data property and a getter property.
let C = [];
for (let i = 0; i < NCLASS; ++i) {
    let klass = class {
        get THIS() { return this; }
    };
    klass.prototype.prop = i;

    C.push(klass);
}

// Derive class using super property access
class D extends C[0] {
    get prop() { return super.prop; }
    get elem() { return super[g_prop]; }

    get prop_this() { return super.THIS; }
    get elem_this() { return super[g_THIS]; }
}

let d = new D();

for (var j = 0; j < 4; ++j) {
    for (var i = 0; i < 15; ++i) {
        // Change base class by overriding [[HomeObject]].[[Prototype]]
        Object.setPrototypeOf(D.prototype, C[j].prototype);

        // Check we get property of correct class
        assertEq(d.prop, j);
        assertEq(d.elem, j);

        // Check super getter gets |this| of object not base class
        assertEq(d.prop_this, d);
        assertEq(d.elem_this, d);
    }
}
