var g = newGlobal();

var f;
f = cloneAndExecuteScript('(function(x) { return x })', g);
assertEq(f(9), 9);

f = cloneAndExecuteScript('(function(x) { { let y = x+1; return y } })', g);
assertEq(f(9), 10);

f = cloneAndExecuteScript('(function(x) { { let y = x, z = 1; return y+z } })', g);
assertEq(f(9), 10);

f = cloneAndExecuteScript('(function(x) { return x.search(/ponies/) })', g);
assertEq(f('123ponies'), 3);

f = cloneAndExecuteScript('(function(x, y) { return x.search(/a/) + y.search(/b/) })', g);
assertEq(f('12a','foo'), 1);

f = cloneAndExecuteScript('(function(x) { switch(x) { case "a": return "b"; case null: return "c" } })', g);
assertEq(f('a'), "b");
assertEq(f(null), "c");
assertEq(f(3), undefined);
