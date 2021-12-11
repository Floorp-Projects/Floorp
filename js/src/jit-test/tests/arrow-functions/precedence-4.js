// Funny case that looks kind of like default arguments isn't.

var f = (msg='hi', w=window => w.alert(a, b));  // sequence expression
assertEq(msg, 'hi');
assertEq(typeof w, 'function');
assertEq(f, w);
