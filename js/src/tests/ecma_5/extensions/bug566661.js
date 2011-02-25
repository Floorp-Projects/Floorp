var f = function (q) { return q['\xC7']; }
var d = eval(uneval(f));
assertEq(d({'\xC7': 'good'}), 'good');

if (typeof reportCompare === "function")
    reportCompare(true, true);
