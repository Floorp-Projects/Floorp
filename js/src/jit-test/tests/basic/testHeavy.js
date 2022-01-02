function heavyFn1(i) { 
    if (i == 3) {
	var x = 3;
        return [0, i].map(function (i) { return i + x; });
    }
    return [];
}

function testHeavy() {
    for (var i = 0; i <= 3; i++)
        heavyFn1(i);
}

testHeavy();
