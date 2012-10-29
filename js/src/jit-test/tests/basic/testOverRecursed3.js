// |jit-test| error:InternalError

var x = [];
x.push(x, x); // more than one so the sort can't be optimized away
x.toString = x.sort;
x.toString();
