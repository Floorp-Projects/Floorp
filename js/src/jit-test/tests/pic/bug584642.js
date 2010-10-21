// |jit-test| error: ReferenceError
Function("x=[(x)=s]")();
/* Don't assert. */
