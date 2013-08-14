// |jit-test| ion-eager

function causeBreak(t, n, r) {
    gcPreserveCode();
    gc();
}

function centralizeGetProp(p)
{
    p.someProp;
}

var handler = {};

function test() {
    var p = new Proxy({}, handler);

    var count = 5;
    for (var i = 0; i < count; i++) {
        centralizeGetProp(p);
    }
    handler.get = causeBreak;
    centralizeGetProp(p);
}

test();
