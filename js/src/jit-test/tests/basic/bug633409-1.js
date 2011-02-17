// vim: set ts=4 sw=4 tw=99 et:
x = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 };

for (i in x)
    delete x.d;

x = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6 };
y = [];
for (i in x)
    y.push(i)
    
assertEq(y[3], "d");

