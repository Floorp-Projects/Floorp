var obj = {p: 100};
var name = "p";
var a = [];
for (var i = 0; i < 10; i++)
    a[i] = ++obj[name];
assertEq(a.join(','), '101,102,103,104,105,106,107,108,109,110');
assertEq(obj.p, 110);
