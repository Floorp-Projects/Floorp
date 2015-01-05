var lfcode = new Array();
lfcode.push = function(x) { eval("(function() { " + x + " })();"); };
lfcode.push("\
function error(str) { try { eval(str); } catch (e) { return e; } }\
const YIELD_PAREN = error('(function*(){(for (y of (yield 1, 2)) y)})').message;\
const GENEXP_YIELD = error('(function*(){(for (x of yield 1) x)})').message;\
const GENERIC = error('(for)').message;\
const eval = [];\
");
