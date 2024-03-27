
// This tests a case where TokenStreamAnyChars::fillExceptingContext
// mishandled a wasm frame, leading to an assertion failure.

if (!wasmIsSupported())
    quit();

const v0 = `
    const o6 = {
        f() {
            function F2() {
                if (!new.target) { throw 'must be called with new'; }
            }
            return F2();
            return {}; // This can be anything, but it must be present
        },
    };

    const o7 = {
        "main": o6,
    };

    const v15 = new WebAssembly.Module(wasmTextToBinary(\`
    (module
        (import "main" "f" (func))
        (func (export "go")
            call 0
        )
    )\`));
    const v16 = new WebAssembly.Instance(v15, o7);
    v16.exports.go();
`;

const o27 = {
    // Both "fileName" and null are necessary
    "fileName": null,
};

let caught = false;
try {
    evaluate(v0, o27);
} catch (e) {
    assertEq(e, "must be called with new");
    caught = true;
}
assertEq(caught, true);
