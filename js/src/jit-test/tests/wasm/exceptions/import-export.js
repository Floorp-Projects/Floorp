// Tests for Wasm exception import and export.

// The WebAssembly.Tag constructor cannot be called for now until the
// JS API specifies the behavior. Same with WebAssembly.Exception.
function testException() {
  assertErrorMessage(
    () => new WebAssembly.Tag(),
    WebAssembly.RuntimeError,
    /cannot call WebAssembly.Tag/
  );
}

function testImports() {
  var mod = `
    (module
      (type (func (param i32 i32)))
      (import "m" "exn" (event (type 0))))
 `;

  assertErrorMessage(
    () => wasmEvalText(mod, { m: { exn: "not a tag" } }),
    WebAssembly.LinkError,
    /import object field 'exn' is not a Tag/
  );
}

function testExports() {
  var exports1 = wasmEvalText(`
    (module (type (func)) (event (export "exn") (type 0)))
  `).exports;

  assertEq(typeof exports1.exn, "object");
  assertEq(exports1.exn instanceof WebAssembly.Tag, true);

  var exports2 = wasmEvalText(`
    (module
      (type (func (param i32 i32)))
      (event (export "exn") (type 0)))
  `).exports;

  assertEq(typeof exports2.exn, "object");
  assertEq(exports2.exn instanceof WebAssembly.Tag, true);
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
    /imported tag 'm.exn' signature mismatch/
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
  assertEq(imports[0].kind, "tag");

  assertEq(exports[0].name, "e");
  assertEq(exports[0].kind, "tag");
}

testException();
testImports();
testExports();
testImportExport();
testDescriptions();
