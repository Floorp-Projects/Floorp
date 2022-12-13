// |jit-test| error: ReferenceError

gczeal(0);
enqueueMark('set-color-gray');
enqueueMark({});
gczeal(9);
gczeal(11, 2);
a;
