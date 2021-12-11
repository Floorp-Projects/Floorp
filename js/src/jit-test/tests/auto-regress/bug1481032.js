x = [];
x[6] = 0;
Object.preventExtensions(x);

// Don't assert.
x.length = 1;
