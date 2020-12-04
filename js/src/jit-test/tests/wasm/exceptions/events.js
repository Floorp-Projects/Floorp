// Tests for event section support

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
    (event $exn (type 0)))
`);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    { name: eventId, body: [] },
  ]),
  WebAssembly.CompileError,
  /expected number of events/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    { name: eventId, body: [1, 1] },
  ]),
  WebAssembly.CompileError,
  /illegal event kind/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    { name: eventId, body: [1, 0] },
  ]),
  WebAssembly.CompileError,
  /expected function index in event/
);

wasmEval(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    eventSection([{ type: 0 }]),
  ])
);

wasmError(
  moduleWithSections([
    sigSection([badExnType]),
    memorySection(0),
    eventSection([{ type: 0 }]),
  ]),
  WebAssembly.CompileError,
  /exception function types must not return anything/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    memorySection(0),
    eventSection([{ type: 1 }]),
  ]),
  WebAssembly.CompileError,
  /function type index in event out of bounds/
);

wasmError(
  moduleWithSections([
    sigSection([emptyType]),
    eventSection([{ type: 0 }]),
    memorySection(0),
  ]),
  WebAssembly.CompileError,
  /expected custom section/
);
