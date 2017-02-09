// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

var BUGNUMBER = 1331009;
var summary = "Newline is allowed between await and operand";

print(BUGNUMBER + ": " + summary);

var expr = async function foo() {
    return await
    10;
};
assertEventuallyEq(expr(), 10);

if (typeof reportCompare === "function")
    reportCompare(true, true);
