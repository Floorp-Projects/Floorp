// |jit-test| skip-if: !('oomTest' in this)

let s = saveStack();
oomTest(() => { saveStack(); });
