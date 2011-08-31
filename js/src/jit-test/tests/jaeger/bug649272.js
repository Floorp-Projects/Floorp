function f(x) {return x;}
x = f(/abc/);
eval("this.__defineSetter__(\"x\", function(){}); x = 3;");
eval("var BUGNUMBER = 233483;");
