// |jit-test| error:TypeError
x = [0, 0];
x.shift();
x.pop();
Object.preventExtensions(x);
x.unshift(0);
