function test(i) {
    return i * 0 + 0;
}

for(var i=0; i<100; i++){
    var x = test(-i);
    assertEq((x===0 && (1/x)===-Infinity), false); // value should be 0, not -0
}

function test2(i) {
    return 0 - i;
}

for(var i=-100; i<100; i++){
    var x = test2(-i);
    assertEq(x, i);
}

