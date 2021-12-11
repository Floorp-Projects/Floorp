x = 0
for (a = 0; a < 3; a++) {
    new ((function () {
        return Float64Array
    })())(x, 1)
}
