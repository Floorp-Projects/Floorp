// |jit-test| error: ReferenceError

gczeal(0);
setMarkStackLimit(1);
gczeal(4);
a;
