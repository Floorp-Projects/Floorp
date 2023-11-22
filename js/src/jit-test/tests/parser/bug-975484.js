var loc = Reflect.parse("f()").body[0].expression.loc;
assertEq(loc.start.column, 1);
assertEq(loc.end.column, 4);

loc = Reflect.parse("f(x)").body[0].expression.loc;
assertEq(loc.start.column, 1);
assertEq(loc.end.column, 5);
