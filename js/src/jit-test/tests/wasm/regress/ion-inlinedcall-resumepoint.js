var invalidatedFunction = function() { return false; }

var counter = 0;

function maybeInvalidate(iplusk) {
    if (iplusk === 0) {
        // This should happen only once; it will invalidate the call frame of
        // the ion() function, which should rewind to just after the wasm call
        // (if it got its own resume point).
        // Before the patch, the wasm call doesn't get its resume point, so
        // it's repeated and the importedFunc() is called once too many.
        counter++;
        invalidatedFunction = function () { return true; };
    }
}

function importedFunc(k) {
    for (let i = 100; i --> 0 ;) {
        maybeInvalidate(i + k);
    }
}

let { exports } = new WebAssembly.Instance(
    new WebAssembly.Module(
        wasmTextToBinary(`
        (module
            (func $imp (import "env" "importedFunc") (param i32))
            (func (export "exp") (param i32)
                local.get 0
                call $imp
            )
        )
        `)
    ), {
        env: {
            importedFunc
        }
    }
);

function ion(k) {
    exports.exp(k);
    invalidatedFunction();
}

for (let k = 100; k --> 0 ;) {
    ion(k);
}

assertEq(counter, 1);
