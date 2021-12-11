// Call_Scripted
let f = function(a, b, c, d, e) {
    return a * 10000 + b * 1000 + c * 100 + d * 10 + e;
};
for (let i = 0; i < 4; i++) {
    assertEq(f(...[1, 2, 3, 4, 5]), 12345);
}

// Call_Scripted (constructing)
let A = function(a, b, c, d, e) {
    this.v = a * 10000 + b * 1000 + c * 100 + d * 10 + e;
};
for (let i = 0; i < 4; i++) {
    assertEq(new A(...[1, 2, 3, 4, 5]).v, 12345);
}

// Call_Native
for (let i = 0; i < 4; i++) {
    assertEq(Math.max(...[1, 2, 3, 4, 5]), 5);
}

// Call_Native (constructing)
for (let i = 0; i < 4; i++) {
    assertEq(new Date(...[2014, 4, 28, 8, 16, 1]).getTime(),
             new Date(2014, 4, 28, 8, 16, 1).getTime());
}
