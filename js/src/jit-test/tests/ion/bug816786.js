var g;
function test(a, b) {

    g = 0;
    for(var i=0; i<100; i++) {
        g += i
    }

    var t = a*b;

    for(var i=0; i<100; i++) {
        t = x.y + t;
        return t;
    }

}

function negzero(x) {
  return x===0 && (1/x)===-Infinity;
}


var x = {y:0};
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
        x.y = -0
    }
}
