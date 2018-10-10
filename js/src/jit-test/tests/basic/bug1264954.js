// |jit-test| skip-if: !('oomTest' in this)
function f(x) {
    oomTest(() => eval(x));
}
f("");
f("");
f(`eval([   "x = \`\${new Error.lineNumber}" ].join())`);
