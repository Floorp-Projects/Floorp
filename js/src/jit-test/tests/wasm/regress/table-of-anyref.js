// Faulty prebarrier for tables.
gczeal(4, 8);
let ins = wasmEvalText(
    `(module
       (table (export "t") 10 externref)
       (func (export "set_externref") (param i32) (param externref)
         (table.set (get_local 0) (get_local 1))))`);
ins.exports.set_externref(3, {});
ins.exports.set_externref(3, null);
