// |reftest| skip-if(!xulRuntime.shell)

enableLastWarning();

var g = newGlobal();
g.eval("({}).watch('x', function(){})");

var warning = getLastWarning();
assertEq(warning.name, "Warning");
assertEq(warning.message.includes("watch"), true, "warning should mention watch");

clearLastWarning();

g = newGlobal();
g.eval("({}).unwatch('x')");

warning = getLastWarning();
assertEq(warning.name, "Warning");
assertEq(warning.message.includes("watch"), true, "warning should mention watch");

clearLastWarning();

reportCompare(0, 0);
