function BigInteger() { }
function bnpCopyTo(g) {
    var this_array = g.array;
    for (var i = g.t; i >= 0; --i)
        ;
    g.t = g.t;
}
function bnpFromString(n) {
    n.t = 0;
    var i = 100;
    while (--i >= 0) {
        n.t++;
    }
}
n = new BigInteger();
n.array = new Array();
bnpFromString(n);

g = new BigInteger();
g.array = new Array();
g.t = 100;
bnpCopyTo(g);
