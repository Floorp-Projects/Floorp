// |jit-test| need-for-each

// Binary: cache/js-dbg-32-f59568ec0513-linux
// Flags: -j
//
for each(let y in [0, 0]) {
    eval("\
        for each(e in[0,0,0,0,0,0,0,0]) {\
            x = undefined\
        }\
    ")
}
