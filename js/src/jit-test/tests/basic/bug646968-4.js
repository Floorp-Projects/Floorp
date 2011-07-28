var s = '', x = {a: 1, b: 2, c: 3};
for (let x in eval('x'))
    s += x;
assertEq(s, 'abc');
