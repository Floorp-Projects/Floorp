a = 0
b = [
    a,
    new Float64Array
]
for (c = 0; c < 10000; ++c) b[c & 1][0] = true
