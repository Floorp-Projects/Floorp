// |trace-test| error: invalid arguments

__defineGetter__('x', Float32Array);
with(this)
    x;
