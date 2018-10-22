// |jit-test| skip-if: !('oomTest' in this)

oomTest(() => import("module1.js"));
oomTest(() => import("cyclicImport1.js"));
