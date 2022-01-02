function sieve() {
    for (var i=0; i<100; i++) { }
}
sieve();
gc();

function fib(n) {
    if (n < 2)
        return 1;
    return fib(n-2) + fib(n-1);
}
fib(20);
