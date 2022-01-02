function test() {
  let instance = wasmEvalText(`
    (module
      (global $g (mut externref) (ref.null extern))
      (func (export "set") (param externref) local.get 0 global.set $g)
    )
  `).exports;
  let obj = { field: null };
  instance.set(obj);
  for (var v4 = 0; v4 < 10; v4++) {}
}
test();
