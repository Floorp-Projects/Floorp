// |jit-test| --dump-bytecode

function f() { }
evaluate('function g() { f(); }');
for (var i = 0; i < 2; i++)
    g(0);
