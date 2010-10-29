function testCallPick() {
    function g(x,a) {
        x.f();
    }

    var x = [];
    x.f = function() { }

    var y = [];
    y.f = function() { }

    var z = [x,x,x,x,x,y,y,y,y,y];

    for (var i = 0; i < 10; ++i)
        g.call(this, z[i], "");
    return true;
}
assertEq(testCallPick(), true);
