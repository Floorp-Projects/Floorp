var obj = {s: ""};
var name = "s";
var a = [];
for (var i = 0; i <= RECORDLOOP + 5; i++) {
    a[i] = 'x';
    if (i > RECORDLOOP)
        a[i] = ++obj[name];  // first recording changes obj.s from string to number
}
assertEq(a.join(','), Array(RECORDLOOP + 2).join('x,') + '1,2,3,4,5');
assertEq(obj.s, 5);

