enableGeckoProfilingWithSlowAssertions();

function testBasic() {
    var arr = [2, -1];
    var cmp = function(x, y) {
        readGeckoProfilingStack();
        return x - y;
    };
    for (var i = 0; i < 20; i++) {
        arr.sort(cmp);
    }
}
testBasic();

function testRectifierFrame() {
    var arr = [2, -1];
    var cmp = function(x, y, z, a) {
        readGeckoProfilingStack();
        return x - y;
    };
    for (var i = 0; i < 20; i++) {
        arr.sort(cmp);
    }
}
testRectifierFrame();

function testRectifierFrameCaller() {
    var o = {};
    var calls = 0;
    Object.defineProperty(o, "length", {get: function() {
        calls++;
        readGeckoProfilingStack();
        return 0;
    }});
    for (var i = 0; i < 20; i++) {
        Array.prototype.sort.call(o);
    }
    assertEq(calls, 20);
}
testRectifierFrameCaller();
