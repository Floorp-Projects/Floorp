function testClosureCreationAndInvocation() {
    var a = 'foobar';
    function makeaddv(vvvv) {
        var z = -4 * vvvv;
        var y = -3 * vvvv;
        var x = -2 * vvvv;
        var w = -1 * vvvv;
        var v = 0 * vvvv;
        var u = 1 * vvvv;
        var t = 2 * vvvv;
        var s = 3 * vvvv;
        var r = 4 * vvvv;
        var q = 5 * vvvv;
        var p = 6 * vvvv;
        var o = 7 * vvvv;
        var n = 8 * vvvv;
        var m = 9 * vvvv;
        var l = 10 * vvvv;
        var k = 11 * vvvv;
        var j = 12 * vvvv;
        var i = 13 * vvvv;
        var h = 14 * vvvv;
        var g = 15 * vvvv;
        var f = 16 * vvvv;
        var e = 17 * vvvv;
        var d = 18 * vvvv;
        var c = 19 * vvvv;
        var b = 20 * vvvv;
        var a = 21 * vvvv;
        return function (x) {
                  switch (x) {
                  case 0: return a; case 1: return b;
                  case 2: return c; case 3: return d;
                  case 4: return e; case 5: return f;
                  case 6: return g; case 7: return h;
                  case 8: return i; case 9: return j;
                  case 10: return k; case 11: return l;
                  case 12: return m; case 13: return n;
                  case 14: return o; case 15: return p;
                  case 16: return q; case 17: return r;
                  case 18: return s; case 19: return t;
                  case 20: return u; case 21: return v;
                  case 22: return w; case 23: return x;
                  case 24: return y; case 25: return z;
                  }
                };
    }
    var a = Array();
    for (var i = 0; i < 26; ++i) {
        a.push(makeaddv(Math.random()));
    }
    return a;
}

var a = testClosureCreationAndInvocation();
for (var i = 0; i < 26; ++i) {
    print(a[i](i));
}


