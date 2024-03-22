// Check proper handling of OOM after toQuotedString().

oomTest(function () {
  new WebAssembly.Instance(
    new WebAssembly.Module(wasmTextToBinary('(import "m" "f" (func $f))')),
    {}
  );
});
