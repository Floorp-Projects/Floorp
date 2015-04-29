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
	    assertEq(b.includes(`getgname "${v}"`), hasIt);
	    assertEq(b.includes(`getname "${v}"`), !hasIt);
	}
    } catch (e) {
	print(e.stack);
	throw e;
    }
}

var x = "outer";

setLazyParsingDisabled(true);
{
    let x = "inner";
    eval("function g() { assertEq(x, 'inner');} g()");
    eval("function g2() { (function nest() { assertEq(x, 'inner'); })(); } g2()");
}
eval(`
     function g3() {
	 assertEq(x, 'outer');
     }
     g3();
     hasGname(g3, 'x');
     `);
eval(`
     function g4() {
	 function nest() { assertEq(x, 'outer'); }
	 nest();
	 return nest;
     }
     hasGname(g4(), 'x');
     `);
setLazyParsingDisabled(false);

{
    let x = "inner";
    eval("function h() { assertEq(x, 'inner');} h()");
    eval("function h2() { (function nest() { assertEq(x, 'inner'); })(); } h2()");
}
// It sure would be nice if we could run the h3/h4 tests below, but it turns out
// that lazy functions and eval don't play together all that well.  See bug
// 1146080.  For now, assert we have no gname, so people will notice if they
// accidentally fix it and adjust this test accordingly.
eval(`
     function h3() {
	 assertEq(x, 'outer');
     }
     h3();
     hasGname(h3, 'x', false);
     `);
eval(`
     function h4() {
	 function nest() { assertEq(x, 'outer'); }
	 nest();
	 return nest;
     }
     hasGname(h4(), 'x', false);
     `);

setLazyParsingDisabled(true);
with ({}) {
    let x = "inner";
    eval("function i() { assertEq(x, 'inner');} i()");
    eval("function i2() { (function nest() { assertEq(x, 'inner'); })(); } i2()");
}
setLazyParsingDisabled(false);

with ({}) {
    let x = "inner";
    eval("function j() { assertEq(x, 'inner');} j()");
    eval("function j2() { (function nest() { assertEq(x, 'inner'); })(); } j2()");
}

setLazyParsingDisabled(true);
(function () {
    var x = "inner";
    eval("function k() { assertEq(x, 'inner');} k()");
    eval("function k2() { (function nest() { assertEq(x, 'inner'); })(); } k2()");
})();
setLazyParsingDisabled(false);

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

setLazyParsingDisabled(true);

var y3 = 5;
eval(`
     'use strict';
     var y3 = 6;
     assertEq(y3, 6);
     (function() { assertEq(y3, 6); })()
     `);
assertEq(y3, 5);

eval(`
     'use strict';
     var y4 = 6;
     assertEq(y4, 6);
     (function() { assertEq(y4, 6); })()
     `);

setLazyParsingDisabled(false);
