if (!wasmGcEnabled() || typeof WebAssembly.Global !== 'function') {
    quit(0);
}

// Dummy object.
function Baguette(calories) {
    this.calories = calories;
}

assertEq(new WebAssembly.Global({value: "anyref"}) instanceof WebAssembly.Global, true);

(function() {
    // Test initialization without a value.
    let g = new WebAssembly.Global({value: "anyref"});
    assertEq(g.value, null);
    assertErrorMessage(() => g.value = 42, TypeError, /immutable global/);
})();

(function() {
    // Test initialization with a value.
    let g = new WebAssembly.Global({value: "anyref"}, null);
    assertEq(g.value, null);
    assertErrorMessage(() => g.value = 42, TypeError, /immutable global/);

    let obj = {};
    g = new WebAssembly.Global({value: "anyref"}, obj);
    assertEq(g.value, obj);
    assertErrorMessage(() => g.value = 42, TypeError, /immutable global/);

    g = new WebAssembly.Global({value: "anyref"}, 1337);
    assertEq(g.value instanceof Number, true);
    assertEq(+g.value, 1337);

    g = new WebAssembly.Global({value: "anyref"}, 13.37);
    assertEq(g.value instanceof Number, true);
    assertEq(+g.value, 13.37);

    g = new WebAssembly.Global({value: "anyref"}, "string");
    assertEq(g.value instanceof String, true);
    assertEq(g.value.toString(), "string");

    g = new WebAssembly.Global({value: "anyref"}, true);
    assertEq(g.value instanceof Boolean, true);
    assertEq(!!g.value, true);

    g = new WebAssembly.Global({value: "anyref"}, Symbol("status"));
    assertEq(g.value instanceof Symbol, true);
    assertEq(g.value.toString(), "Symbol(status)");

    assertErrorMessage(() => new WebAssembly.Global({value: "anyref"}, undefined),
                       TypeError,
                       "can't convert undefined to object");
})();

(function() {
    // Test mutable property and assignment.
    let g = new WebAssembly.Global({value: "anyref", mutable: true}, null);
    assertEq(g.value, null);

    let obj = { x: 42 };
    g.value = obj;
    assertEq(g.value, obj);
    assertEq(g.value.x, 42);

    obj = null;
    assertEq(g.value.x, 42);

    let otherObj = { y : 35 };
    g.value = otherObj;
    assertEq(g.value, otherObj);
})();

(function() {
    // Test tracing.
    let nom = new Baguette(1);
    let g = new WebAssembly.Global({value: "anyref"}, nom);
    nom = null;
    gc();
    assertEq(g.value.calories, 1);
})();

var global = new WebAssembly.Global({ value: "anyref", mutable: true }, null);

// GCZeal mode 2 implies that every allocation (second parameter = every single
// allocation) will trigger a full GC.
gczeal(2, 1);

{
    let nomnom = new Baguette(42);
    global.value = nomnom;
    nomnom = null;
}
new Baguette();
assertEq(global.value.calories, 42);
