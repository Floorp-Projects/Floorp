// Binary: cache/js-dbg-64-a37127f33d22-linux
// Flags: -m -n
//
a = {}
b = __proto__
for (i = 0; i < 9; i++) {
    __proto__ = a
    a.__proto__ = b
}
