function f(o) {
    var res = 0;
    for (var i=0; i<11000; i++) {
        res += o.x;
    }
    return res;
}

function O(x) {
    if (x)
        this.x = 10;
}

f(new O(true));

// "o.x" is now missing so the idempotent cache should invalidate f.
f(new O(false));
