const { Instance, Module, LinkError } = WebAssembly;

// Locally-defined globals
assertErrorMessage(() => wasmEvalText(`(module (global))`), SyntaxError, /parsing/);
assertErrorMessage(() => wasmEvalText(`(module (global i32))`), SyntaxError, /parsing/);
assertErrorMessage(() => wasmEvalText(`(module (global (mut i32)))`), SyntaxError, /parsing/);

// Initializer expressions.
wasmFailValidateText(`(module (global i32 (f32.const 13.37)))`, /type mismatch/);
wasmFailValidateText(`(module (global f64 (f32.const 13.37)))`, /type mismatch/);
wasmFailValidateText(`(module (global i32 (i32.add (i32.const 13) (i32.const 37))))`, /failed to read end/);

wasmFailValidateText(`(module (global i32 (get_global 0)))`, /out of range/);
wasmFailValidateText(`(module (global i32 (get_global 1)) (global i32 (i32.const 1)))`, /out of range/);

// Test a well-defined global section.
function testInner(type, initialValue, nextValue, coercion)
{
    var module = wasmEvalText(`(module
        (global (mut ${type}) (${type}.const ${initialValue}))
        (global ${type} (${type}.const ${initialValue}))

        (func $get (result ${type}) (get_global 0))
        (func $set (param ${type}) (set_global 0 (get_local 0)))

        (func $get_cst (result ${type}) (get_global 1))

        (export "get" $get)
        (export "get_cst" $get_cst)

        (export "set" $set)
    )`).exports;

    assertEq(module.get(), coercion(initialValue));
    assertEq(module.set(coercion(nextValue)), undefined);
    assertEq(module.get(), coercion(nextValue));

    assertEq(module.get_cst(), coercion(initialValue));
}

testInner('i32', 13, 37, x => x|0);
testInner('f32', 13.37, 0.1989, Math.fround);
testInner('f64', 13.37, 0.1989, x => +x);

// Semantic errors.
wasmFailValidateText(`(module (global (mut i32) (i32.const 1337)) (func (set_global 1 (i32.const 0))))`, /out of range/);
wasmFailValidateText(`(module (global i32 (i32.const 1337)) (func (set_global 0 (i32.const 0))))`, /can't write an immutable global/);

// Big module with many variables: test that setting one doesn't overwrite the
// other ones.
function get_set(i, type) {
    return `
        (func $get_${i} (result ${type}) (get_global ${i}))
        (func $set_${i} (param ${type}) (set_global ${i} (get_local 0)))
    `;
}

var module = wasmEvalText(`(module
    (global (mut i32) (i32.const 42))
    (global (mut i32) (i32.const 10))
    (global (mut f32) (f32.const 13.37))
    (global (mut f64) (f64.const 13.37))
    (global (mut i32) (i32.const -18))

    ${get_set(0, 'i32')}
    ${get_set(1, 'i32')}
    ${get_set(2, 'f32')}
    ${get_set(3, 'f64')}
    ${get_set(4, 'i32')}

    (export "get0" $get_0) (export "set0" $set_0)
    (export "get1" $get_1) (export "set1" $set_1)
    (export "get2" $get_2) (export "set2" $set_2)
    (export "get3" $get_3) (export "set3" $set_3)
    (export "get4" $get_4) (export "set4" $set_4)
)`).exports;

let values = [42, 10, Math.fround(13.37), 13.37, -18];
let nextValues = [13, 37, Math.fround(-17.89), 9.3, -13];
for (let i = 0; i < 5; i++) {
    assertEq(module[`get${i}`](), values[i]);
    assertEq(module[`set${i}`](nextValues[i]), undefined);
    assertEq(module[`get${i}`](), nextValues[i]);
    for (let j = 0; j < 5; j++) {
        if (i === j)
            continue;
        assertEq(module[`get${j}`](), values[j]);
    }
    assertEq(module[`set${i}`](values[i]), undefined);
    assertEq(module[`get${i}`](), values[i]);
}

// Initializer expressions can also be used in elem section initializers.
wasmFailValidateText(`(module (import "globals" "a" (global f32)) (table 4 anyfunc) (elem (get_global 0) $f) (func $f))`, /type mismatch/);

module = wasmEvalText(`(module
    (import "globals" "a" (global i32))
    (table (export "tbl") 4 anyfunc)
    (elem (get_global 0) $f)
    (func $f)
    (export "f" $f)
)`, {
    globals: {
        a: 1
    }
}).exports;
assertEq(module.f, module.tbl.get(1));

// Import/export rules.
if (typeof WebAssembly.Global === "undefined") {
    wasmFailValidateText(`(module (import "globals" "x" (global (mut i32))))`,
             /can't import.* mutable globals in the MVP/);
    wasmFailValidateText(`(module (global (mut i32) (i32.const 42)) (export "" global 0))`,
             /can't .*export mutable globals in the MVP/);
}

// Import/export semantics.
module = wasmEvalText(`(module
 (import $g "globals" "x" (global i32))
 (func $get (result i32) (get_global $g))
 (export "getter" $get)
 (export "value" global 0)
)`, { globals: {x: 42} }).exports;

assertEq(module.getter(), 42);
// Adapt to ongoing experiment with WebAssembly.Global.
// assertEq() will not trigger @@toPrimitive, so we must have a cast here.
if (typeof WebAssembly.Global === "function")
    assertEq(Number(module.value), 42);
else
    assertEq(module.value, 42);

// Can only import numbers (no implicit coercions).
module = new Module(wasmTextToBinary(`(module
    (global (import "globs" "i32") i32)
    (global (import "globs" "f32") f32)
    (global (import "globs" "f64") f32)
)`));

const assertLinkFails = (m, imp, err) => {
    assertErrorMessage(() => new Instance(m, imp), LinkError, err);
}

var imp = {
    globs: {
        i32: 0,
        f32: Infinity,
        f64: NaN
    }
};

let i = new Instance(module, imp);

for (let v of [
    null,
    {},
    "42",
    /not a number/,
    false,
    undefined,
    Symbol(),
    { valueOf() { return 42; } }
]) {
    imp.globs.i32 = v;
    assertLinkFails(module, imp, /not a Number/);

    imp.globs.i32 = 0;
    imp.globs.f32 = v;
    assertLinkFails(module, imp, /not a Number/);

    imp.globs.f32 = Math.fround(13.37);
    imp.globs.f64 = v;
    assertLinkFails(module, imp, /not a Number/);

    imp.globs.f64 = 13.37;
}

// Imported globals and locally defined globals use the same index space.
module = wasmEvalText(`(module
 (import "globals" "x" (global i32))
 (global i32 (i32.const 1337))
 (export "imported" global 0)
 (export "defined" global 1)
)`, { globals: {x: 42} }).exports;

// See comment earlier about WebAssembly.Global
if (typeof WebAssembly.Global === "function") {
    assertEq(Number(module.imported), 42);
    assertEq(Number(module.defined), 1337);
} else {
    assertEq(module.imported, 42);
    assertEq(module.defined, 1337);
}

// Initializer expressions can reference an imported immutable global.
wasmFailValidateText(`(module (global f32 (f32.const 13.37)) (global i32 (get_global 0)))`, /must reference a global immutable import/);
wasmFailValidateText(`(module (global (mut f32) (f32.const 13.37)) (global i32 (get_global 0)))`, /must reference a global immutable import/);
wasmFailValidateText(`(module (global (mut i32) (i32.const 0)) (global i32 (get_global 0)))`, /must reference a global immutable import/);

wasmFailValidateText(`(module (import "globals" "a" (global f32)) (global i32 (get_global 0)))`, /type mismatch/);

function testInitExpr(type, initialValue, nextValue, coercion, assertFunc = assertEq) {
    var module = wasmEvalText(`(module
        (import "globals" "a" (global ${type}))

        (global $glob_mut (mut ${type}) (get_global 0))
        (global $glob_imm ${type} (get_global 0))

        (func $get0 (result ${type}) (get_global 0))

        (func $get1 (result ${type}) (get_global 1))
        (func $set1 (param ${type}) (set_global 1 (get_local 0)))

        (func $get_cst (result ${type}) (get_global 2))

        (export "get0" $get0)
        (export "get1" $get1)
        (export "get_cst" $get_cst)

        (export "set1" $set1)
        (export "global_imm" (global $glob_imm))
    )`, {
        globals: {
            a: coercion(initialValue)
        }
    }).exports;

    assertFunc(module.get0(), coercion(initialValue));
    assertFunc(module.get1(), coercion(initialValue));
    // See comment earlier about WebAssembly.Global
    if (typeof WebAssembly.Global === "function")
        assertFunc(Number(module.global_imm), coercion(initialValue));
    else
        assertFunc(module.global_imm, coercion(initialValue));

    assertEq(module.set1(coercion(nextValue)), undefined);
    assertFunc(module.get1(), coercion(nextValue));
    assertFunc(module.get0(), coercion(initialValue));
    // See comment earlier about WebAssembly.Global
    if (typeof WebAssembly.Global === "function")
        assertFunc(Number(module.global_imm), coercion(initialValue));
    else
        assertFunc(module.global_imm, coercion(initialValue));

    assertFunc(module.get_cst(), coercion(initialValue));
}

testInitExpr('i32', 13, 37, x => x|0);
testInitExpr('f32', 13.37, 0.1989, Math.fround);
testInitExpr('f64', 13.37, 0.1989, x => +x);

// Int64.

// Import and export

// The test for a Number value dominates the guard against int64.
assertErrorMessage(() => wasmEvalText(`(module
                                        (import "globals" "x" (global i64)))`,
                                      {globals: {x:false}}),
                   LinkError,
                   /import object field 'x' is not a Number/);

// The imported value is a Number, so the int64 guard should stop us
assertErrorMessage(() => wasmEvalText(`(module
                                        (import "globals" "x" (global i64)))`,
                                      {globals: {x:42}}),
                   LinkError,
                   /cannot pass i64 to or from JS/);

if (typeof WebAssembly.Global === "undefined") {

    // Cannot export int64 at all.

    assertErrorMessage(() => wasmEvalText(`(module
                                            (global i64 (i64.const 42))
                                            (export "" global 0))`),
                       LinkError,
                       /cannot pass i64 to or from JS/);

} else {

    // We can import and export i64 globals as cells.  They cannot be created
    // from JS because there's no way to specify a non-zero initial value; that
    // restriction is tested later.  But we can export one from a module and
    // import it into another.

    let i = wasmEvalText(`(module
                           (global (export "g") i64 (i64.const 37))
                           (global (export "h") (mut i64) (i64.const 37)))`);

    let j = wasmEvalText(`(module
                           (import "globals" "g" (global i64))
                           (func (export "f") (result i32)
                            (i64.eq (get_global 0) (i64.const 37))))`,
                         {globals: {g: i.exports.g}});

    assertEq(j.exports.f(), 1);

    // We cannot read or write i64 global values from JS.

    let g = i.exports.g;

    assertErrorMessage(() => i.exports.g.value, TypeError, /cannot pass i64 to or from JS/);

    // Mutability check comes before i64 check.
    assertErrorMessage(() => i.exports.g.value = 12, TypeError, /can't set value of immutable global/);
    assertErrorMessage(() => i.exports.h.value = 12, TypeError, /cannot pass i64 to or from JS/);
}

// Test inner
var initialValue = '0x123456789abcdef0';
var nextValue = '0x531642753864975F';
wasmAssert(`(module
    (global (mut i64) (i64.const ${initialValue}))
    (global i64 (i64.const ${initialValue}))
    (func $get (result i64) (get_global 0))
    (func $set (param i64) (set_global 0 (get_local 0)))
    (func $get_cst (result i64) (get_global 1))
    (export "get" $get)
    (export "get_cst" $get_cst)
    (export "set" $set)
)`, [
    {type: 'i64', func: '$get', expected: initialValue},
    {type: 'i64', func: '$set', args: [`i64.const ${nextValue}`]},
    {type: 'i64', func: '$get', expected: nextValue},
    {type: 'i64', func: '$get_cst', expected: initialValue},
]);

// Custom NaN.
{
    let dv = new DataView(new ArrayBuffer(8));
    module = wasmEvalText(`(module
     (global $g f64 (f64.const -nan:0xe7ffff1591120))
     (global $h f32 (f32.const -nan:0x651234))
     (export "nan64" (global $g))(export "nan32" (global $h))
    )`, {}).exports;

    dv.setFloat64(0, module.nan64, true);
    assertEq(dv.getUint32(4, true), 0x7ff80000);
    assertEq(dv.getUint32(0, true), 0x00000000);

    dv.setFloat32(0, module.nan32, true);
    assertEq(dv.getUint32(0, true), 0x7fc00000);
}

// WebAssembly.Global experiment

if (typeof WebAssembly.Global === "function") {

    const Global = WebAssembly.Global;

    // These types should work:
    assertEq(new Global({type: "i32"}) instanceof Global, true);
    assertEq(new Global({type: "f32"}) instanceof Global, true);
    assertEq(new Global({type: "f64"}) instanceof Global, true);

    // These types should not work:
    assertErrorMessage(() => new Global({type: "i64"}),   TypeError, /bad type for a WebAssembly.Global/);
    assertErrorMessage(() => new Global({}),              TypeError, /bad type for a WebAssembly.Global/);
    assertErrorMessage(() => new Global({type: "fnord"}), TypeError, /bad type for a WebAssembly.Global/);
    assertErrorMessage(() => new Global(),                TypeError, /Global requires more than 0 arguments/);

    // Coercion of init value; ".value" accessor
    assertEq((new Global({type: "i32"}, 3.14)).value, 3);
    assertEq((new Global({type: "f32"}, { valueOf: () => 33.5 })).value, 33.5);
    assertEq((new Global({type: "f64"}, "3.25")).value, 3.25);

    // Nothing special about NaN, it coerces just fine
    assertEq((new Global({type: "i32"}, NaN)).value, 0);

    // The default init value is zero.
    assertEq((new Global({type: "i32"})).value, 0);
    assertEq((new Global({type: "f32"})).value, 0);
    assertEq((new Global({type: "f64"})).value, 0);

    {
        // "value" is enumerable
        let x = new Global({type: "i32"});
        let s = "";
        for ( let i in x )
            s = s + i + ",";
        assertEq(s, "value,");
    }

    // "value" is defined on the prototype, not on the object
    assertEq("value" in Global.prototype, true);

    // Can't set the value of an immutable global
    assertErrorMessage(() => (new Global({type: "i32"})).value = 10,
                       TypeError,
                       /can't set value of immutable global/);

    {
        // Can set the value of a mutable global
        let g = new Global({type: "i32", mutable: true}, 37);
        g.value = 10;
        assertEq(g.value, 10);
    }

    {
        // Misc internal conversions
        let g = new Global({type: "i32"}, 42);

        // valueOf
        assertEq(g - 5, 37);

        // @@toStringTag
        assertEq(g.toString(), "[object WebAssembly.Global]");
    }

    {
        // An exported global should appear as a Global instance:
        let i = wasmEvalText(`(module (global (export "g") i32 (i32.const 42)))`);

        assertEq(typeof i.exports.g, "object");
        assertEq(i.exports.g instanceof Global, true);

        // An exported global can be imported into another instance even if
        // it is an object:
        let j = wasmEvalText(`(module
                               (global (import "" "g") i32)
                               (func (export "f") (result i32)
                                (get_global 0)))`,
                             { "": { "g": i.exports.g }});

        // And when it is then accessed it has the right value:
        assertEq(j.exports.f(), 42);
    }

    // Identity of Global objects (independent of mutablity).
    {
        // When a global is exported twice, the two objects are the same.
        let i = wasmEvalText(`(module
                               (global i32 (i32.const 0))
                               (export "a" global 0)
                               (export "b" global 0))`);
        assertEq(i.exports.a, i.exports.b);

        // When a global is imported and then exported, the exported object is
        // the same as the imported object.
        let j = wasmEvalText(`(module
                               (import "" "a" (global i32))
                               (export "x" global 0))`,
                             { "": {a: i.exports.a}});

        assertEq(i.exports.a, j.exports.x);

        // When a global is imported twice (ie aliased) and then exported twice,
        // the exported objects are the same, and are also the same as the
        // imported object.
        let k = wasmEvalText(`(module
                               (import "" "a" (global i32))
                               (import "" "b" (global i32))
                               (export "x" global 0)
                               (export "y" global 1))`,
                             { "": {a: i.exports.a,
                                    b: i.exports.a}});

        assertEq(i.exports.a, k.exports.x);
        assertEq(k.exports.x, k.exports.y);
    }

    // Mutability
    {
        let i = wasmEvalText(`(module
                               (global (export "g") (mut i32) (i32.const 37))
                               (func (export "getter") (result i32)
                                (get_global 0))
                               (func (export "setter") (param i32)
                                (set_global 0 (get_local 0))))`);

        let j = wasmEvalText(`(module
                               (import "" "g" (global (mut i32)))
                               (func (export "getter") (result i32)
                                (get_global 0))
                               (func (export "setter") (param i32)
                                (set_global 0 (get_local 0))))`,
                             {"": {g: i.exports.g}});

        // Initial values
        assertEq(i.exports.g.value, 37);
        assertEq(i.exports.getter(), 37);
        assertEq(j.exports.getter(), 37);

        // Set in i, observe everywhere
        i.exports.setter(42);

        assertEq(i.exports.g.value, 42);
        assertEq(i.exports.getter(), 42);
        assertEq(j.exports.getter(), 42);

        // Set in j, observe everywhere
        j.exports.setter(78);

        assertEq(i.exports.g.value, 78);
        assertEq(i.exports.getter(), 78);
        assertEq(j.exports.getter(), 78);

        // Set on global object, observe everywhere
        i.exports.g.value = 197;

        assertEq(i.exports.g.value, 197);
        assertEq(i.exports.getter(), 197);
        assertEq(j.exports.getter(), 197);
    }

    // Mutability of import declaration and imported value have to match
    {
        const mutErr = /imported global mutability mismatch/;
        const i64Err = /cannot pass i64 to or from JS/;

        let m1 = new Module(wasmTextToBinary(`(module
                                               (import "m" "g" (global i32)))`));

        // Mutable Global matched to immutable import
        let gm = new Global({type: "i32", mutable: true}, 42);
        assertErrorMessage(() => new Instance(m1, {m: {g: gm}}),
                           LinkError,
                           mutErr);

        let m2 = new Module(wasmTextToBinary(`(module
                                               (import "m" "g" (global (mut i32))))`));

        // Immutable Global matched to mutable import
        let gi = new Global({type: "i32", mutable: false}, 42);
        assertErrorMessage(() => new Instance(m2, {m: {g: gi}}),
                           LinkError,
                           mutErr);

        // Constant value is the same as immutable Global
        assertErrorMessage(() => new Instance(m2, {m: {g: 42}}),
                           LinkError,
                           mutErr);

        let m3 = new Module(wasmTextToBinary(`(module
                                               (import "m" "g" (global (mut i64))))`));

        // Check against i64 import before matching mutability
        assertErrorMessage(() => new Instance(m3, {m: {g: 42}}),
                           LinkError,
                           i64Err);
    }

    // TEST THIS LAST

    // "value" is deletable
    assertEq(delete Global.prototype.value, true);
    assertEq("value" in Global.prototype, false);

    // ADD NO MORE TESTS HERE!
}
