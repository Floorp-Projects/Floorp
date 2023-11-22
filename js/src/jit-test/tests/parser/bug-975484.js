var loc = Reflect.parse("f()").body[0].expression.loc;
assertEq(loc.start.column, 0);
assertEq(loc.end.column, 3);

loc = Reflect.parse("f(x)").body[0].expression.loc;
assertEq(loc.start.column, 0);
assertEq(loc.end.column, 4);
