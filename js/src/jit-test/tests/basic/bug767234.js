function gen()
{
    var local = new Date();
    yield 1;
    local = null;
    gc();
    gcslice(0);
    yield 2;
}

var g = gen();
g.next();
g.next();
gcslice();
