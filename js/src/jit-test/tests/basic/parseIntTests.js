function parseIntHelper(n) {
    var a;
    for (var i = 0; i < 5; i++)
      a = parseInt(n);
    return a;
}
function doParseIntTests() {
    var inputs = [0, -0, .1, -.1, .7, -.7, 1.3, -1.3];
    var outputs = new Array(8);
    //avoid jit, unrolled
    outputs[0] = outputs[1] = outputs[2] = outputs[4] = 0;
    outputs[3] = outputs[5] = -0;
    outputs[6] = 1;
    outputs[7] = -1;
    for (var i = 0; i < 8; i++) {
        var n = Object.is(inputs[i], -0) ? "-0" : String(inputs[i]);
        var testfn = new Function('return parseIntHelper(' + n + ');');
        assertEq(testfn(), outputs[i]);
    }
}
doParseIntTests();

assertEq(parseInt("08"), 8);
assertEq(parseInt("09"), 9);
