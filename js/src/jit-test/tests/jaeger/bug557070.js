// |jit-test| error: InternalError

for (e in (function x() { [eval()].some(x) } ()));

/* Don't crash or assert. */

