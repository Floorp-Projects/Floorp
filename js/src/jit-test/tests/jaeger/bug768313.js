// |jit-test| mjit; mjitalways; dump-bytecode

function f() { }
evaluate('function g() { f(); }', {newContext: true});
for (var i = 0; i < 2; i++)
    g(0);
