// Tests for Wasm exception import and export.

// The WebAssembly.Exception constructor cannot be called for now until the
// JS API specifies the behavior.
function testException() {
  assertErrorMessage(
    () => new WebAssembly.Exception(),
    WebAssembly.RuntimeError,
    /cannot call WebAssembly.Exception/
  );
}

function testImports() {
  var mod = `
    (module
      (type (func (param i32 i32)))
      (import "m" "exn" (event (type 0))))
 `;

  assertErrorMessage(
    () => wasmEvalText(mod, { m: { exn: "not an exception" } }),
    WebAssembly.LinkError,
    /import object field 'exn' is not a Exception/
  );
}

function testExports() {
  var exports1 = wasmEvalText(`
    (module (type (func)) (event (export "exn") (type 0)))
  `).exports;

  assertEq(typeof exports1.exn, "object");
  assertEq(exports1.exn instanceof WebAssembly.Exception, true);

  var exports2 = wasmEvalText(`
    (module
      (type (func (param i32 i32)))
      (event (export "exn") (type 0)))
  `).exports;

  assertEq(typeof exports2.exn, "object");
  assertEq(exports2.exn instanceof WebAssembly.Exception, true);
}

function testImportExport() {
  var exports = wasmEvalText(`
    (module
      (type (func (param i32)))
      (event (export "exn") (type 0)))
  `).exports;

  wasmEvalText(
    `
    (module
      (type (func (param i32)))
      (import "m" "exn" (event (type 0))))
  `,
    { m: exports }
  );

  assertErrorMessage(
    () => {
      wasmEvalText(
        `
      (module
        (type (func (param)))
        (import "m" "exn" (event (type 0))))
    `,
        { m: exports }
      );
    },
    WebAssembly.LinkError,
    /imported exception 'm.exn' signature mismatch/
  );
}

// Test imports/exports descriptions.
function testDescriptions() {
  const imports = WebAssembly.Module.imports(
    new WebAssembly.Module(
      wasmTextToBinary(`
        (module $m
          (type (func))
          (import "m" "e" (event (type 0))))
      `)
    )
  );

  const exports = WebAssembly.Module.exports(
    new WebAssembly.Module(
      wasmTextToBinary(`
        (module
          (type (func))
          (event (export "e") (type 0)))
      `)
    )
  );

  assertEq(imports[0].module, "m");
  assertEq(imports[0].name, "e");
  assertEq(imports[0].kind, "event");

  assertEq(exports[0].name, "e");
  assertEq(exports[0].kind, "event");
}

testException();
testImports();
testExports();
testImportExport();
testDescriptions();
