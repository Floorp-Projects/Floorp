// |jit-test| error:InternalError

x = [];
x.push(x);
x.toString = x.sort;
x.toString();
