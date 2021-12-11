function f(x) {
    "use asm"
    return !(1 || x)
}
for (var j = 0; j < 1; j++) {
    (function(x) {
        +f(+x)
    })()
}
