// |jit-test| error: TypeError

function* of([d] = eval("var MYVAR=new String('0Xf');++MYVAR"), ...get) { var x = 42;}
of();
