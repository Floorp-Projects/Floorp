var x = {p: 0.1, m: function(){}};
x.m();  // the interpreter brands x
for (var i = 0; i < 9; i++)
    x.p = 0.1;
