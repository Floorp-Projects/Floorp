assertEq(((function() arguments) for (x in [1])).next()(42)[0], 42);
assertEq(((function() {return arguments}) for (x in [1])).next()(42)[0], 42);
assertEq(((function() {return arguments[0] + arguments[1]}) for (x in [1])).next()(41,1), 42);
assertEq(((function() {return arguments[0] + (function() { return arguments[0]})(arguments[1])}) for (x in [1])).next()(41,1), 42);
assertEq(((function() { var arguments = 3; return arguments}) for (x in [1])).next()(42), 3);
