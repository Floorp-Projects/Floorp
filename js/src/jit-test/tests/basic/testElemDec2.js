var obj = {s: ""};
var name = "s";
for (var i = 0; i <= RECORDLOOP + 5; i++)
    if (i > RECORDLOOP)
        obj[name]--;  // first recording changes obj.s from string to number
assertEq(obj.s, -5);

