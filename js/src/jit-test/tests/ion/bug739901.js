function List(l) {
    this.l = l;
};

function f(p) {
    return g(p);
};

function g(p) {
    if (p instanceof List)
        return f(p.l);
    return dumpStack().length;
};

var l = new List(new List(new List(new List(null))));

for (let i = 0; i < 3000; i++)
    assertEq(g(l), 10);

try{} catch (x) {}
