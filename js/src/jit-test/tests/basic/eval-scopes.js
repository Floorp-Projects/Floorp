function bytecode(f) {
    if (typeof disassemble !== "function")
        return "unavailable";
    var d = disassemble(f);
    return d.slice(d.indexOf("main:"), d.indexOf("\n\n"));
}

function hasGname(f, v, hasIt = true) {
    // Do a try-catch that prints the full stack, so we can tell
    // _which_ part of this test failed.
    try {
        var b = bytecode(f);
        if (b != "unavailable") {
            assertEq(b.includes(`GetGName "${v}"`), hasIt);
            assertEq(b.includes(`GetName "${v}"`), !hasIt);
        }
    } catch (e) {
        print(e.stack);
        throw e;
    }
}

var x = "outer";

{
    let x = "inner";
    eval("function h() { assertEq(x, 'inner');} h()");
    eval("function h2() { (function nest() { assertEq(x, 'inner'); })(); } h2()");
}

// GNAME optimizations should work through lazy parsing.
eval(`
     function h3() {
         assertEq(x, 'outer');
     }
     h3();
     hasGname(h3, 'x', true);
     `);
eval(`
     function h4() {
         function nest() { assertEq(x, 'outer'); }
         nest();
         return nest;
     }
     hasGname(h4(), 'x', true);
     `);

with ({}) {
    let x = "inner";
    eval("function j() { assertEq(x, 'inner');} j()");
    eval("function j2() { (function nest() { assertEq(x, 'inner'); })(); } j2()");
}

(function () {
    let x = "inner";
    eval("function l() { assertEq(x, 'inner');} l()");
    eval("function l2() { (function nest() { assertEq(x, 'inner'); })(); } l2()");
})();

var y1 = 5;
eval(`
     'use strict';
     var y1 = 6;
     assertEq(y1, 6);
     (function() { assertEq(y1, 6); })()
     `);
assertEq(y1, 5);

eval(`
     'use strict';
     var y2 = 6;
     assertEq(y2, 6);
     (function() { assertEq(y2, 6); })()
     `);
