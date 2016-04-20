if (!('oomTest' in this))
    quit();

print = function() {}
function k() dissrc(print);
function j() k();
function h() j();
function f() h();
f();
oomTest(() => f())
