var obj = {s: ""};
var name = "s";
var a = [];
for (var i = 0; i <= 13; i++) {
    a[i] = 'x';
    if (i > 8)
        a[i] = ++obj[name];  // first recording changes obj.s from string to number
}
assertEq(a.join(','), Array(10).join('x,') + '1,2,3,4,5');
assertEq(obj.s, 5);

