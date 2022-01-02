function heavyFn1(i) { 
    if (i == 3) {
	var x = 3;
        return [0, i].map(function (i) { return i + x; });
    }
    return [];
}

function heavyFn2(i) {
    if (i < 1000)
        return heavyFn1(i);
    return function () { return i; };
}

function testHeavy2() {
    for (var i = 0; i <= 3; i++)
        heavyFn2(i);
}

testHeavy2();
