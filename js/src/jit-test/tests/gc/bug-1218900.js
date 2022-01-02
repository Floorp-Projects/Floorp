// |jit-test| --fuzzing-safe
readline = function() {};
Function.prototype.toString = function() {
    for (var i = 0; i < 2; i++) {
        this()
    }
};
getBacktrace({thisprops: true});
