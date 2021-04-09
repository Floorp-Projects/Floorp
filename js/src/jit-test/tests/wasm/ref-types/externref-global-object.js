// |jit-test| skip-if: typeof WebAssembly.Global !== 'function'

// Dummy object.
function Baguette(calories) {
    this.calories = calories;
}

assertEq(new WebAssembly.Global({value: "externref"}) instanceof WebAssembly.Global, true);

(function() {
    // Test initialization without a value.
    let g = new WebAssembly.Global({value: "externref"});
    assertEq(g.value, null);
    assertErrorMessage(() => g.value = 42, TypeError, /immutable global/);
})();

(function() {
    // Test initialization with a value.
    let g = new WebAssembly.Global({value: "externref"}, null);
    assertEq(g.value, null);
    assertErrorMessage(() => g.value = 42, TypeError, /immutable global/);

    let obj = {};
    g = new WebAssembly.Global({value: "externref"}, obj);
    assertEq(g.value, obj);
    assertErrorMessage(() => g.value = 42, TypeError, /immutable global/);

    g = new WebAssembly.Global({value: "externref"}, 1337);
    assertEq(typeof g.value, "number");
    assertEq(+g.value, 1337);

    g = new WebAssembly.Global({value: "externref"}, 13.37);
    assertEq(typeof g.value, "number");
    assertEq(+g.value, 13.37);

    g = new WebAssembly.Global({value: "externref"}, "string");
    assertEq(typeof g.value, "string");
    assertEq(g.value.toString(), "string");

    g = new WebAssembly.Global({value: "externref"}, true);
    assertEq(typeof g.value, "boolean");
    assertEq(!!g.value, true);

    g = new WebAssembly.Global({value: "externref"}, Symbol("status"));
    assertEq(typeof g.value, "symbol");
    assertEq(g.value.toString(), "Symbol(status)");

    g = new WebAssembly.Global({value: "externref"}, undefined);
    assertEq(g.value, undefined);
})();

(function() {
    // Test mutable property and assignment.
    let g = new WebAssembly.Global({value: "externref", mutable: true}, null);
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
    let g = new WebAssembly.Global({value: "externref"}, nom);
    nom = null;
    gc();
    assertEq(g.value.calories, 1);
})();

var global = new WebAssembly.Global({ value: "externref", mutable: true }, null);

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
