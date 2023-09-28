// |jit-test| test-also=--wasm-extended-const; test-also=--no-wasm-extended-const

const { Instance, Module, LinkError } = WebAssembly;

// Locally-defined globals
assertErrorMessage(() => wasmEvalText(`(module (global))`), SyntaxError, /wasm text error/);
// A global field in the text format is valid with an empty expression, but this produces an invalid module
assertErrorMessage(() => wasmEvalText(`(module (global i32))`), WebAssembly.CompileError, /popping value/);
assertErrorMessage(() => wasmEvalText(`(module (global (mut i32)))`), WebAssembly.CompileError, /popping value/);

// Initializer expressions.
wasmFailValidateText(`(module (global i32 (f32.const 13.37)))`, /type mismatch/);
wasmFailValidateText(`(module (global f64 (f32.const 13.37)))`, /type mismatch/);

wasmFailValidateText(`(module (global i32 (global.get 0)))`, /out of range/);
wasmFailValidateText(`(module (global i32 (global.get 1)) (global i32 (i32.const 1)))`, /out of range/);

// Test a well-defined global section.
function testInner(type, initialValue, nextValue, coercion)
{
    var module = wasmEvalText(`(module
        (global (mut ${type}) (${type}.const ${initialValue}))
        (global ${type} (${type}.const ${initialValue}))

        (func $get (result ${type}) (global.get 0))
        (func $set (param ${type}) (global.set 0 (local.get 0)))

        (func $get_cst (result ${type}) (global.get 1))

        (export "get" (func $get))
        (export "get_cst" (func $get_cst))

        (export "set" (func $set))
    )`).exports;

    assertEq(module.get(), coercion(initialValue));
    assertEq(module.set(coercion(nextValue)), undefined);
    assertEq(module.get(), coercion(nextValue));

    assertEq(module.get_cst(), coercion(initialValue));
}

testInner('i32', 13, 37, x => x|0);
testInner('f32', 13.37, 0.1989, Math.fround);
testInner('f64', 13.37, 0.1989, x => +x);

// Extended const stuff
if (wasmExtendedConstEnabled()) {
    // Basic global shenanigans
    {
        const module = wasmEvalText(`(module
            ;; -2 * (5 - (-10 + 20)) = 10
            (global i32 (i32.mul (i32.const -2) (i32.sub (i32.const 5) (i32.add (i32.const -10) (i32.const 20)))))
            ;; ((1 + 2) - (3 * 4)) = -9
            (global i64 (i64.sub (i64.add (i64.const 1) (i64.const 2)) (i64.mul (i64.const 3) (i64.const 4))))

            (func (export "get0") (result i32) global.get 0)
            (func (export "get1") (result i64) global.get 1)
        )`).exports;

        assertEq(module.get0(), 10);
        assertEq(module.get1(), -9n);
    }

    // Example use of dynamic linking
    {
        // Make a memory for two dynamically-linked modules to share. Each module gets five pages.
        const mem = new WebAssembly.Memory({ initial: 15, maximum: 15 });

        const mod1 = new WebAssembly.Module(wasmTextToBinary(`(module
            (memory (import "env" "memory") 15 15)
            (global $memBase (import "env" "__memory_base") i32)
            (data (offset (global.get $memBase)) "Hello from module 1.")
            (data (offset (i32.add (global.get $memBase) (i32.const 65536))) "Goodbye from module 1.")
        )`));
        const instance1 = new WebAssembly.Instance(mod1, {
            env: {
                memory: mem,
                __memory_base: 65536 * 5, // this module's memory starts at page 5
            },
        });

        const mod2 = new WebAssembly.Module(wasmTextToBinary(`(module
            (memory (import "env" "memory") 15 15)
            (global $memBase (import "env" "__memory_base") i32)
            (data (offset (global.get $memBase)) "Hello from module 2.")
            (data (offset (i32.add (global.get $memBase) (i32.const 65536))) "Goodbye from module 2.")
        )`));
        const instance2 = new WebAssembly.Instance(mod2, {
            env: {
                memory: mem,
                __memory_base: 65536 * 10, // this module's memory starts at page 10
            },
        });

        // All four strings should now be present in the memory.

        function assertStringInMem(mem, str, addr) {
            const bytes = new Uint8Array(mem.buffer).slice(addr, addr + str.length);
            let memStr = String.fromCharCode(...bytes);
            assertEq(memStr, str);
        }

        assertStringInMem(mem, "Hello from module 1.", 65536 * 5);
        assertStringInMem(mem, "Goodbye from module 1.", 65536 * 6);
        assertStringInMem(mem, "Hello from module 2.", 65536 * 10);
        assertStringInMem(mem, "Goodbye from module 2.", 65536 * 11);
    }
}

// Semantic errors.
wasmFailValidateText(`(module (global (mut i32) (i32.const 1337)) (func (global.set 1 (i32.const 0))))`, /(out of range)|(global index out of bounds)/);
wasmFailValidateText(`(module (global i32 (i32.const 1337)) (func (global.set 0 (i32.const 0))))`, /(can't write an immutable global)|(global is immutable)/);

// Big module with many variables: test that setting one doesn't overwrite the
// other ones.
function get_set(i, type) {
    return `
        (func $get_${i} (result ${type}) (global.get ${i}))
        (func $set_${i} (param ${type}) (global.set ${i} (local.get 0)))
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

    (export "get0" (func $get_0)) (export "set0" (func $set_0))
    (export "get1" (func $get_1)) (export "set1" (func $set_1))
    (export "get2" (func $get_2)) (export "set2" (func $set_2))
    (export "get3" (func $get_3)) (export "set3" (func $set_3))
    (export "get4" (func $get_4)) (export "set4" (func $set_4))
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
wasmFailValidateText(`(module (import "globals" "a" (global f32)) (table 4 funcref) (elem (global.get 0) $f) (func $f))`, /type mismatch/);

module = wasmEvalText(`(module
    (import "globals" "a" (global i32))
    (table (export "tbl") 4 funcref)
    (elem (global.get 0) $f)
    (func $f)
    (export "f" (func $f))
)`, {
    globals: {
        a: 1
    }
}).exports;
assertEq(module.f, module.tbl.get(1));

// Import/export semantics.
module = wasmEvalText(`(module
 (import "globals" "x" (global $g i32))
 (func $get (result i32) (global.get $g))
 (export "getter" (func $get))
 (export "value" (global 0))
)`, { globals: {x: 42} }).exports;

assertEq(module.getter(), 42);

// assertEq() will not trigger @@toPrimitive, so we must have a cast here.
assertEq(Number(module.value), 42);

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
 (export "imported" (global 0))
 (export "defined" (global 1))
)`, { globals: {x: 42} }).exports;

assertEq(Number(module.imported), 42);
assertEq(Number(module.defined), 1337);

if (!wasmGcEnabled()) {
  // Initializer expressions can reference an imported immutable global.
  wasmFailValidateText(`(module (global f32 (f32.const 13.37)) (global i32 (global.get 0)))`, /must reference a global immutable import/);
  wasmFailValidateText(`(module (global (mut f32) (f32.const 13.37)) (global i32 (global.get 0)))`, /must reference a global immutable import/);
  wasmFailValidateText(`(module (global (mut i32) (i32.const 0)) (global i32 (global.get 0)))`, /must reference a global immutable import/);
}

wasmFailValidateText(`(module (import "globals" "a" (global f32)) (global i32 (global.get 0)))`, /type mismatch/);

function testInitExpr(type, initialValue, nextValue, coercion, assertFunc = assertEq) {
    var module = wasmEvalText(`(module
        (import "globals" "a" (global ${type}))

        (global $glob_mut (mut ${type}) (global.get 0))
        (global $glob_imm ${type} (global.get 0))

        (func $get0 (result ${type}) (global.get 0))

        (func $get1 (result ${type}) (global.get 1))
        (func $set1 (param ${type}) (global.set 1 (local.get 0)))

        (func $get_cst (result ${type}) (global.get 2))

        (export "get0" (func $get0))
        (export "get1" (func $get1))
        (export "get_cst" (func $get_cst))

        (export "set1" (func $set1))
        (export "global_imm" (global $glob_imm))
    )`, {
        globals: {
            a: coercion(initialValue)
        }
    }).exports;

    assertFunc(module.get0(), coercion(initialValue));
    assertFunc(module.get1(), coercion(initialValue));
    assertFunc(Number(module.global_imm), coercion(initialValue));

    assertEq(module.set1(coercion(nextValue)), undefined);
    assertFunc(module.get1(), coercion(nextValue));
    assertFunc(module.get0(), coercion(initialValue));
    assertFunc(Number(module.global_imm), coercion(initialValue));

    assertFunc(module.get_cst(), coercion(initialValue));
}

testInitExpr('i32', 13, 37, x => x|0);
testInitExpr('f32', 13.37, 0.1989, Math.fround);
testInitExpr('f64', 13.37, 0.1989, x => +x);

// Int64.

// Import and export

// Test inner
var initialValue = '0x123456789abcdef0';
var nextValue = '0x531642753864975F';
wasmAssert(`(module
    (global (mut i64) (i64.const ${initialValue}))
    (global i64 (i64.const ${initialValue}))
    (func $get (result i64) (global.get 0))
    (func $set (param i64) (global.set 0 (local.get 0)))
    (func $get_cst (result i64) (global.get 1))
    (export "get" (func $get))
    (export "get_cst" (func $get_cst))
    (export "set" (func $set))
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

// WebAssembly.Global
{
    const Global = WebAssembly.Global;

    // These types should work:
    assertEq(new Global({value: "i32"}) instanceof Global, true);
    assertEq(new Global({value: "f32"}) instanceof Global, true);
    assertEq(new Global({value: "f64"}) instanceof Global, true);
    assertEq(new Global({value: "i64"}) instanceof Global, true); // No initial value works

    // Coercion of init value; ".value" accessor
    assertEq((new Global({value: "i32"}, 3.14)).value, 3);
    assertEq((new Global({value: "f32"}, { valueOf: () => 33.5 })).value, 33.5);
    assertEq((new Global({value: "f64"}, "3.25")).value, 3.25);

    // Nothing special about NaN, it coerces just fine
    assertEq((new Global({value: "i32"}, NaN)).value, 0);

    // The default init value is zero.
    assertEq((new Global({value: "i32"})).value, 0);
    assertEq((new Global({value: "f32"})).value, 0);
    assertEq((new Global({value: "f64"})).value, 0);
    let mod = wasmEvalText(`(module
                             (import "" "g" (global i64))
                             (func (export "f") (result i32)
                              (i64.eqz (global.get 0))))`,
                           {"":{g: new Global({value: "i64"})}});
    assertEq(mod.exports.f(), 1);

    {
        // "value" is enumerable
        let x = new Global({value: "i32"});
        let s = "";
        for ( let i in x )
            s = s + i + ",";
        if (getBuildConfiguration("release_or_beta")) {
            assertEq(s, "valueOf,value,");
        } else {
            assertEq(s, "type,valueOf,value,");
        }
    }

    // "value" is defined on the prototype, not on the object
    assertEq("value" in Global.prototype, true);

    // Can't set the value of an immutable global
    assertErrorMessage(() => (new Global({value: "i32"})).value = 10,
                       TypeError,
                       /can't set value of immutable global/);

    {
        // Can set the value of a mutable global
        let g = new Global({value: "i32", mutable: true}, 37);
        g.value = 10;
        assertEq(g.value, 10);
    }

    {
        // Misc internal conversions
        let g = new Global({value: "i32"}, 42);

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
                                (global.get 0)))`,
                             { "": { "g": i.exports.g }});

        // And when it is then accessed it has the right value:
        assertEq(j.exports.f(), 42);
    }

    // Identity of Global objects (independent of mutablity).
    {
        // When a global is exported twice, the two objects are the same.
        let i = wasmEvalText(`(module
                               (global i32 (i32.const 0))
                               (export "a" (global 0))
                               (export "b" (global 0)))`);
        assertEq(i.exports.a, i.exports.b);

        // When a global is imported and then exported, the exported object is
        // the same as the imported object.
        let j = wasmEvalText(`(module
                               (import "" "a" (global i32))
                               (export "x" (global 0)))`,
                             { "": {a: i.exports.a}});

        assertEq(i.exports.a, j.exports.x);

        // When a global is imported twice (ie aliased) and then exported twice,
        // the exported objects are the same, and are also the same as the
        // imported object.
        let k = wasmEvalText(`(module
                               (import "" "a" (global i32))
                               (import "" "b" (global i32))
                               (export "x" (global 0))
                               (export "y" (global 1)))`,
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
                                (global.get 0))
                               (func (export "setter") (param i32)
                                (global.set 0 (local.get 0))))`);

        let j = wasmEvalText(`(module
                               (import "" "g" (global (mut i32)))
                               (func (export "getter") (result i32)
                                (global.get 0))
                               (func (export "setter") (param i32)
                                (global.set 0 (local.get 0))))`,
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

        let m1 = new Module(wasmTextToBinary(`(module
                                               (import "m" "g" (global i32)))`));

        // Mutable Global matched to immutable import
        let gm = new Global({value: "i32", mutable: true}, 42);
        assertErrorMessage(() => new Instance(m1, {m: {g: gm}}),
                           LinkError,
                           mutErr);

        let m2 = new Module(wasmTextToBinary(`(module
                                               (import "m" "g" (global (mut i32))))`));

        // Immutable Global matched to mutable import
        let gi = new Global({value: "i32", mutable: false}, 42);
        assertErrorMessage(() => new Instance(m2, {m: {g: gi}}),
                           LinkError,
                           mutErr);

        // Constant value is the same as immutable Global
        assertErrorMessage(() => new Instance(m2, {m: {g: 42}}),
                           LinkError,
                           mutErr);
    }

    // TEST THIS LAST

    // "value" is deletable
    assertEq(delete Global.prototype.value, true);
    assertEq("value" in Global.prototype, false);

    // ADD NO MORE TESTS HERE!
}

// Standard wat syntax: the parens around the initializer expression are
// optional.
{
    let i1 = wasmEvalText(
        `(module
           (global $g i32 i32.const 37)
           (func (export "f") (result i32) (global.get $g)))`);
    assertEq(i1.exports.f(), 37);

    let i2 = wasmEvalText(
        `(module
           (global $g (mut f64) f64.const 42.0)
           (func (export "f") (result f64) (global.get $g)))`);
    assertEq(i2.exports.f(), 42);

    let i3 = wasmEvalText(
        `(module
           (global $x (import "m" "x") i32)
           (global $g i32 global.get $x)
           (func (export "f") (result i32) (global.get $g)))`,
        {m:{x:86}});
    assertEq(i3.exports.f(), 86);
}
