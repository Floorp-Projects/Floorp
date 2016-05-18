// |jit-test| allow-oom
try {
    x = '';
    for (var y in this) {
        x += x + 'z';
    };
} catch (e) {}
evaluate('', ({
    sourceMapURL: x
}));
