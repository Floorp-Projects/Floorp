// |jit-test| error:ReferenceError

// Binary: cache/js-opt-64-5a04fd69aa09-linux
// Flags: --ion-eager
//

function deep1(x) {
    if (0) {  }
    else i : dumpStack();
}
for (var i = 0; 1; ++i)
    deep1(i);
