function a(a, b, c, g) {
    for (;;) {
        if (0 > c) return a;
        a: {
            for (;;) {
                var k = a.forward[c];
                if (t(k))
                    if (k.key < b) a = k;
                    else break a;
                else break a
            }
            a = void 0
        }
        null !=
            g && (g[c] = a);
        c -= 1
    }
}

function t(a) {
    return null != a && !1 !== a
}


var d = {forward: [{},null,{}]}
for (var i=0; i < 1000; i++) {
    a(d, 0, 1, null);
    a(d, 0, 0, null);
}




function test(a) {
  var t = a[0]
  if (t) {
    return t.test;
  }
}

function test2(a) {
  var t = a[0]
  if (t) {
    if (t) {
      return t.test;
    }
  }
}

function test3(a) {
  var t = a[0]
  if (t !== null) {
    if (t !== undefined) {
      return t.test;
    }
  }
}

var a = [{test:1}]
var b = [undefined]
assertEq(test(b), undefined)
assertEq(test(a), 1)
assertEq(test(a), 1)
assertEq(test2(b), undefined)
assertEq(test2(a), 1)
assertEq(test2(a), 1)
assertEq(test3(b), undefined)
assertEq(test3(a), 1)
assertEq(test3(a), 1)
