function test(a, b) {
    var y = a*b;
    return y + y;
}

function negzero(x) {
  return x===0 && (1/x)===-Infinity;
}

var a = 0;
var b = 0;
for(var i=0; i<58; i++) {
    var o = test(a, b);

    // Test returns
    // * 0, if i < 50
    // * -0, if i >= 50
    assertEq(negzero(o), i>50);

    if (i == 50) {
        a = -1
    }
}
