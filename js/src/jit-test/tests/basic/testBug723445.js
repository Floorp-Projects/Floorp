var global = newGlobal('new-compartment');
global.eval("function f(b) { if (b) { new Error }; }");

function f(b) { global.f(b) }
function g(b) { f(b) }
function h() {
    for (var i = 0; i < 1000; ++i)
        g(i > 900);
}
h();
