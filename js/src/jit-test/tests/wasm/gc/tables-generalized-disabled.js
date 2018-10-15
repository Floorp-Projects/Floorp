// |jit-test| skip-if: wasmGeneralizedTables()

assertErrorMessage(() => new WebAssembly.Table({element:"anyref", initial:10}),
                   TypeError,
                   /"element" property of table descriptor must be "anyfunc"/);

