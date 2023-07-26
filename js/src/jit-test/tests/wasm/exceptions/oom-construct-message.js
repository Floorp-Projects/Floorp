// |jit-test| skip-if: !('oomTest' in this)

const tag = new WebAssembly.Tag({ parameters: ["i32"] });
oomTest(() => {
  new WebAssembly.Exception(tag, []);
});
