// |reftest| skip-if(!xulRuntime.shell) -- needs drainJobQueue

var BUGNUMBER = 1185106;
var summary = "arguments.callee in sloppy mode should return wrapped function";

print(BUGNUMBER + ": " + summary);

async function decl1() {
  return arguments.callee;
}
assertEventuallyEq(decl1(), decl1);

var expr1 = async function foo() {
  return arguments.callee;
};
assertEventuallyEq(expr1(), expr1);

var expr2 = async function() {
  return arguments.callee;
};
assertEventuallyEq(expr2(), expr2);

var obj = {
  async method1() {
    return arguments.callee;
  }
};
assertEventuallyEq(obj.method1(), obj.method1);

if (typeof reportCompare === "function")
    reportCompare(true, true);
