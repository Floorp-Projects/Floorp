// |jit-test| error:TypeError

// Binary: cache/js-dbg-64-4e9567eeb09e-linux
// Flags: --ion-eager
//
"".replace(RegExp(), Array.reduce)
