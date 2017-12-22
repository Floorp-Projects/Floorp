// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

var BUGNUMBER = 1185106;
var summary = "Named async function expression should get wrapped function for the name inside it";

print(BUGNUMBER + ": " + summary);

var expr = async function foo() {
  return foo;
};
assertEventuallyEq(expr(), expr);

if (typeof reportCompare === "function")
    reportCompare(true, true);
