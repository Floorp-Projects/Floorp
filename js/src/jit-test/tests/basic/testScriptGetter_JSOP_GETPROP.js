var a = {get p() { return 11; }};

function f() { return a; }

var g = 0;
for (var i = 0; i < 9; i++)
    g += f().p;
assertEq(g, 99);
