setJitCompilerOption('ion.enable', 1);
function g(x) {
    for (let i = 0; i < 2; ++i) {
        var y = x[i];
        print(y >>> y);
    }
}
g([2147483649, -2147483648]);
