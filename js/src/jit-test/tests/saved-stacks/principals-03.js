// With arrows representing child-to-parent links, create a SavedFrame stack
// like this:
//
//     high.a -> low.b
//
// in `low`'s compartment and give `low` a reference to this stack. Assert the
// stack's youngest frame's properties doesn't leak information about `high.a`
// that `low` shouldn't have access to, and instead returns information about
// `low.b`.

var low = newGlobal({ principal: 0 });
var high = newGlobal({ principal: 0xfffff });

low.high = high;
high.low = low;

high.eval("function a() { return saveStack(0, low); }");
low.eval("function b() { return high.a(); }")

var stack = low.b();

assertEq(stack.functionDisplayName, "b");
assertEq(stack.parent, null);
