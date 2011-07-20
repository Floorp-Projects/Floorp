var s, x = 0;

s = '';
for (let x = x; x < 3; x++)
    s += x;
assertEq(s, '012');

s = '';
for (let x = eval('x'); x < 3; x++)
    s += x;
assertEq(s, '012');

s = ''
for (let x = function () { with ({}) return x; }(); x < 3; x++)
    s += x;
assertEq(s, '012');
