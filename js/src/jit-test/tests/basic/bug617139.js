// |jit-test| error: InternalError
// don't assert

gczeal(2)
function x() { 
    [null].some(x)
}
x();

