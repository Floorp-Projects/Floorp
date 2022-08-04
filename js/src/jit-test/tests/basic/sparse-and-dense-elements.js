function f(o) {
    for (var i = 0; i < 40; i++) {
        if ((i > 10 && (i % 2) === 0) || i > 30) {
            Object.defineProperty(o, i, {value: i, enumerable: false,
                                         writable: true, configurable: true});
        } else {
            o[i] = i;
        }
    }
    for (var i = 0; i < 15; i++) {
        var sum = 0;
        for (var j = 0; j < 40; j++) {
            o[j]++;
            sum += o[j];
        }
        assertEq(sum, 820 + i * 40);
    }
}
f({});
f([]);
