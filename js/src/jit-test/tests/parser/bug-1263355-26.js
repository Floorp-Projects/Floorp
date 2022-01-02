// |jit-test| error: TypeError

function test(a, b, c, d, e, {} = "zmi") {
    var r = 0
    r += Math.min(a, b, c, r.script.getLineOffsets(g.line0 + 3), e);
}
test();
