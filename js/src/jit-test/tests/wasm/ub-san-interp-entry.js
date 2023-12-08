var { table } = wasmEvalText(`
  (module (func $add) (table (export "table") 10 funcref) (elem (i32.const 0) $add))
`).exports
table.get(0)();
