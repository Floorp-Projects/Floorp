// Tests for tag section support

load(libdir + "wasm-binary.js");

function wasmEval(code, imports) {
  new WebAssembly.Instance(new WebAssembly.Module(code), imports).exports;
}

function wasmError(code, errorType, regexp) {
  assertErrorMessage(() => wasmEval(code, {}), errorType, regexp);
}

const emptyType = { args: [], ret: VoidCode };
const badExnType = { args: [], ret: I32Code };

wasmEvalText(`
  (module
    (type (func (param i32)))
    (tag $exn (type 0)))
`);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    { name: tagId, body: [] },
  ]),
  WebAssembly.CompileError,
  /expected number of tags/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    { name: tagId, body: [1, 1] },
  ]),
  WebAssembly.CompileError,
  /illegal tag kind/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    { name: tagId, body: [1, 0] },
  ]),
  WebAssembly.CompileError,
  /expected function index in tag/
);

wasmEval(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    tagSection([{ type: 0 }]),
  ])
);

wasmError(
  moduleWithSections([
    sigSection([badExnType]),
    memorySection(0),
    tagSection([{ type: 0 }]),
  ]),
  WebAssembly.CompileError,
  /tag function types must not return anything/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    tagSection([{ type: 1 }]),
  ]),
  WebAssembly.CompileError,
  /function type index in tag out of bounds/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    tagSection([{ type: 0 }]),
    memorySection(0),
  ]),
  WebAssembly.CompileError,
  /expected custom section/
);

(() => {
  const body = [1];
  body.push(...string("mod"));
  body.push(...string("exn"));
  body.push(...varU32(TagCode));

  wasmError(
    moduleWithSections([
      sigSection([emptyType]),
      { name: importId, body: body },
    ]),
    WebAssembly.CompileError,
    /expected tag kind/
  );

  body.push(...varU32(0));
  wasmError(
    moduleWithSections([
      sigSection([emptyType]),
      { name: importId, body: body },
    ]),
    WebAssembly.CompileError,
    /expected function index in tag/
  );

  body.push(...varU32(1));
  wasmError(
    moduleWithSections([
      sigSection([emptyType]),
      { name: importId, body: body },
    ]),
    WebAssembly.CompileError,
    /function type index in tag out of bounds/
  );
})();

wasmEval(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    tagSection([{ type: 0 }]),
    exportSection([{ tagIndex: 0, name: "exn" }]),
  ])
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    tagSection([{ type: 0 }]),
    exportSection([{ tagIndex: 1, name: "exn" }]),
  ]),
  WebAssembly.CompileError,
  /exported tag index out of bounds/
);

(() => {
  const body = [1];
  body.push(...string("exn"));
  body.push(...varU32(TagCode));
  wasmError(
    moduleWithSections([
      sigSection([emptyType]),
      memorySection(0),
      tagSection([{ type: 0 }]),
      { name: exportId, body: body },
    ]),
    WebAssembly.CompileError,
    /expected tag index/
  );
})();
