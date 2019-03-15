// |jit-test| skip-if: !wasmReftypesEnabled()

// Faulty prebarrier for tables.
gczeal(4, 8);
let ins = wasmEvalText(
    `(module
       (table (export "t") 10 anyref)
       (func (export "set_anyref") (param i32) (param anyref)
         (table.set (get_local 0) (get_local 1))))`);
ins.exports.set_anyref(3, {});
ins.exports.set_anyref(3, null);
