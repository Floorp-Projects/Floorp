for (var i = 2; i < 10; i++) {
    assertEq(Math.min(i, 1), 1);
    assertEq(Math.min(i, -1), -1);
    assertEq(Math.min(1, i), 1);
    assertEq(Math.min(-1, i), -1);
    assertEq(Math.min(5, 2), 2);
    assertEq(Math.min(2, 5), 2);
    assertEq(Math.min(5, -2), -2);
    assertEq(Math.min(-2, 5), -2);
}

for (i = 2; i < 10; i++) {
    assertEq(Math.max(i, 1), i);
    assertEq(Math.max(i, -1), i);
    assertEq(Math.max(1, i), i);
    assertEq(Math.max(-1, i), i);
    assertEq(Math.max(5, -2), 5);
    assertEq(Math.max(-2, 5), 5);
    assertEq(Math.max(5, 2), 5);
    assertEq(Math.max(2, 5), 5);
}

for (i = 2.1; i < 13; i += 3.17584) {
    assertEq(Math.max(i, 1), i);
    assertEq(Math.max(i, 1.5), i);
    assertEq(Math.max(1, i), i);
    assertEq(Math.max(1.5, i), i);
    
    assertEq(Math.max(NaN, NaN), NaN);
    assertEq(Math.max(NaN, Infinity), NaN);
    assertEq(Math.max(Infinity, NaN), NaN);
    
    assertEq(Math.max(NaN, i), NaN);
    assertEq(Math.max(i, NaN), NaN);
    
    assertEq(Math.max(i, Infinity), Infinity);
    assertEq(Math.max(Infinity, i), Infinity);
    
    assertEq(Math.max(i, -Infinity), i);
    assertEq(Math.max(-Infinity, i), i);    
}

for (i = 2.1; i < 13; i += 3.17584) {
    assertEq(Math.min(i, 1), 1);
    assertEq(Math.min(i, 1.5), 1.5);
    assertEq(Math.min(1, i), 1);
    assertEq(Math.min(1.5, i), 1.5);
    
    assertEq(Math.min(NaN, NaN), NaN);
    assertEq(Math.min(NaN, Infinity), NaN);
    assertEq(Math.min(Infinity, NaN), NaN);
    
    assertEq(Math.min(NaN, i), NaN);
    assertEq(Math.min(i, NaN), NaN);
    
    assertEq(Math.min(i, Infinity), i);
    assertEq(Math.min(Infinity, i), i);
    
    assertEq(Math.min(i, -Infinity), -Infinity);
    assertEq(Math.min(-Infinity, i), -Infinity);
}

function isNegZero(n) {
    return n === 0 && 1/n === -Infinity;
}

for (i = 0; i < 5; i++) {
    assertEq(isNegZero(Math.min(0, -0)), true);
    assertEq(isNegZero(Math.min(-0, 0)), true);
    assertEq(isNegZero(Math.min(-0, -0)), true);
    assertEq(isNegZero(Math.max(0, -0)), false);
    assertEq(isNegZero(Math.max(-0, 0)), false);
    assertEq(isNegZero(Math.max(-0, -0)), true);
}
