// |jit-test| skip-if: !('oomTest' in this)

const tag = new WebAssembly.Tag({ parameters: ["i32", "i32", "i32", "i32"] });
const params = [0, 0, 0, 0];
oomTest(() => {
  for (var i = 0; i < 5; ++i) {
    new WebAssembly.Exception(tag, params);
  }
});
