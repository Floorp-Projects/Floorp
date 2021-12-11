// |jit-test| error: InternalError
y = 'x'
for (var i=0; i<100; i++)
    y += y;
print(y.length);
