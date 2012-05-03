var g = newGlobal('new-compartment');

g.f = new Function('return function(x) { return x }');
assertEq(g.eval("clone(f)()(9)"), 9);

g.f = new Function('return function(x) { let(y = x+1) { return y } }');
assertEq(g.eval("clone(f)()(9)"), 10);

g.f = new Function('return function(x) { let(y = x, z = 1) { return y+z } }');
assertEq(g.eval("clone(f)()(9)"), 10);

g.f = new Function('return function(x) { return x.search(/ponies/) }');
assertEq(g.eval("clone(f)()('123ponies')"), 3);

g.f = new Function('return function(x,y) { return x.search(/a/) + y.search(/b/) }');
assertEq(g.eval("clone(f)()('12a','foo')"), 1);

g.f = new Function('return [function(x) x+2, function(y) let(z=y+1) z]');
assertEq(g.eval("let ([f,g] = clone(f)()) f(g(4))"), 7);

g.f = new Function('return function(x) { switch(x) { case "a": return "b"; case null: return "c" } }');
assertEq(g.eval("clone(f)()('a')"), "b");
assertEq(g.eval("clone(f)()(null)"), "c");
assertEq(g.eval("clone(f)()(3)"), undefined);
