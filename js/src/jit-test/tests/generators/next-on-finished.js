function*g(){ };
o = g();
o.next();
result = o.next();
assertEq(result.done, true);
assertEq(o.value, undefined);
