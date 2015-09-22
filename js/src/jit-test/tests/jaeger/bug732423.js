enableNoSuchMethod();

function testStuff(x, y) {
    for (var i = 0; i < 60; i++) {
        x[y]();
        x[y];
    }
}
testStuff({"elements":function(){}}, "elements");

var o = {
    res: 0,
    f: function() { this.res += 3; },
    __noSuchMethod__: function() { this.res += 5; }
};

function testNoSuchMethod(x, y) {
    for (var i = 0; i < 60; i++) {
        x[y]();
    }
}

testNoSuchMethod(o, "f");
testNoSuchMethod(o, "g");
assertEq(o.res, 480);
