// |jit-test| error: TypeError

x = [0];
x.length = 9;
Object.freeze(x);
x.reverse();
