x = [];
x.unshift(4, 8);
y = [];
relazifyFunctions();
y[3] = 9;
y.__proto__ = x;
for (var i = 0; i < 2; ++i) {
    y.shift();
}
assertEq(y[0], 8);
