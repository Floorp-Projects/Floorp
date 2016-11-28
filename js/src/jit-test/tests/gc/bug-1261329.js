if (!('oomTest' in this))
    quit();

print = function() {}
function k() { return dissrc(print); }
function j() { return k(); }
function h() { return j(); }
function f() { return h(); }
f();
oomTest(() => f())
