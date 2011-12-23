// |jit-test| error: 3
var str = (function (x) {return (i for (i in x));}).toSource().replace('\n', '');
assertEq(str, "(function (x) {return (i for (i in x));})");
throw 3;
