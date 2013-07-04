// |jit-test| error: TypeError
z = Proxy.create({}, (function(){}));
({__proto__: z, set c(a) {}});
