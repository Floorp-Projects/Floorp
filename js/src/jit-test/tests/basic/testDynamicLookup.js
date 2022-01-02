(function() { var x = 2; eval("assertEq(x, 2)"); })();
(function() { var x = 2; (function() { assertEq(x, 2) })() })();
(function() { var x = 2; (function() { eval("assertEq(x, 2)") })() })();
(function() { var x = 2; (function() { (function() { assertEq(x, 2) })()})() })();
(function() { var x = 2; (function() { (function() { eval("assertEq(x, 2)") })()})() })();

(function() { var x = 2; with({}) { assertEq(x, 2) } })();
(function() { var x = 2; with({}) { (function() { assertEq(x, 2) })() } })();
(function() { var x = 3; with({x:2}) { assertEq(x, 2) } })();
(function() { var x = 3; with({x:2}) { (function() { assertEq(x, 2) })() } })();
(function() { var x = 2; (function() { with({}) { assertEq(x, 2) } })() })();
(function() { var x = 2; (function() { with({}) { (function() { assertEq(x, 2) })() } })() })();
(function() { var x = 3; (function() { with({x:2}) { assertEq(x, 2) } })() })();
(function() { var x = 3; (function() { with({x:2}) { (function() { assertEq(x, 2) })() } })() })();

(function() { if (Math) function x() {}; assertEq(typeof x, "function") })();
(function() { if (Math) function x() {}; eval('assertEq(typeof x, "function")') })();
(function() { if (Math) function x() {}; (function() { assertEq(typeof x, "function") })() })();
(function() { if (Math) function x() {}; (function() { eval('assertEq(typeof x, "function")') })() })();

(function() { eval("var x = 2"); assertEq(x, 2) })();
(function() { eval("var x = 2"); (function() { assertEq(x, 2) })() })();
(function() { eval("var x = 2"); (function() { (function() { assertEq(x, 2) })() })() })();

(function() { var x = 2; (function() { eval('var y = 3'); assertEq(x, 2) })() })();
(function() { var x = 2; (function() { eval('var y = 3'); (function() { assertEq(x, 2) })() })() })();

(function() { var x = 3; (function() { eval('var x = 2'); assertEq(x, 2) })() })();
(function() { var x = 3; (function() { eval('var x = 2'); (function() { assertEq(x, 2) })() })() })();

(function() { var x = 2; eval("eval('assertEq(x, 2)')") })();
(function() { var x = 2; (function() { eval("eval('assertEq(x, 2)')") })() })();
(function() { var x = 2; eval("(function() { eval('assertEq(x, 2)') })()") })();
(function() { var x = 2; (function() { eval("(function() { eval('assertEq(x, 2)') })()") })() })();
(function() { var x = 2; (function() { eval("(function() { eval('(function() { assertEq(x, 2) })()') })()") })() })();

(function() { var [x] = [2]; eval('assertEq(x, 2)') })();
(function() { var [x] = [2]; (function() { assertEq(x, 2) })() })();
(function() { var [x] = [2]; (function() { eval('assertEq(x, 2)') })() })();
(function() { for (var [x] = [2];;) { return (function() { return assertEq(x, 2); })() } })();
(function() { for (var [x] = [2];;) { return (function() { return eval('assertEq(x, 2)'); })() } })();

(function() { var {y:x} = {y:2}; eval('assertEq(x, 2)') })();
(function() { var {y:x} = {y:2}; (function() { assertEq(x, 2) })() })();
(function() { var {y:x} = {y:2}; (function() { eval('assertEq(x, 2)') })() })();
(function() { for (var {y:x} = {y:2};;) { return (function() { return assertEq(x, 2); })() } })();
(function() { for (var {y:x} = {y:2};;) { return (function() { return eval('assertEq(x, 2)'); })() } })();

(function([x]) { eval('assertEq(x, 2)') })([2]);
(function([x]) { (function() { assertEq(x, 2) })() })([2]);
(function([x]) { (function() { eval('assertEq(x, 2)') })() })([2]);

(function f() { assertEq(f.length, 0) })();
(function f() { eval('assertEq(f.length, 0)') })();
(function f() { (function f(x) { eval('assertEq(f.length, 1)') })() })();
(function f() { eval("(function f(x) { eval('assertEq(f.length, 1)') })()") })();

(function f() { arguments = 3; function arguments() {}; assertEq(arguments, 3) })();
(function f() { function arguments() {}; arguments = 3; assertEq(arguments, 3) })();
(function f() { var arguments = 3; function arguments() {}; assertEq(arguments, 3) })();
(function f() { function arguments() {}; var arguments = 3; assertEq(arguments, 3) })();

function f1() { assertEq(typeof f1, "function") }; f1();
with({}) { (function() { assertEq(typeof f1, "function") })() }
if (Math)
    function f2(x) {}
assertEq(f2.length, 1);
