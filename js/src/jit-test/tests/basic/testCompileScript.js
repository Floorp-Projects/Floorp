// |jit-test| mjitalways

var nlocals = 50;
var localstr = "";
for (var i = 0; i < nlocals; ++i)
    localstr += "var x" + i + "; ";

/*
 * Attempt to test, in a stack-parameter-independent manner, ComileFunction
 * hitting a stack-commit boundary (which is not an OOM, but requires checking
 * and updating the stack limit).
 */
var arr = [function() {return 0}, function() {return 1}, function() {return 2}];
var arg = "x";
var body = localstr +
           "if (x == 0) return; " +
           "arr[3] = (new Function(arg, body));" +
           "for (var i = 0; i < 4; ++i) arr[i](x-1);";

// XXX interpreter bailouts during recursion below can cause us to hit the limit quickly.
try { (new Function(arg, body))(1000); } catch (e) {}

/*
 * Also check for OOM in CompileFunction. To avoid taking 5 seconds, use a
 * monster apply to chew up most the stack.
 */
var gotIn = false;
var threwOut = false;
try {
    (function() {
        gotIn = true;
        (new Function(arg, body))(10000000);
     }).apply(null, new Array(getMaxArgs()));
} catch(e) {
    assertEq(""+e, "InternalError: too much recursion");
    threwOut = true;
}
assertEq(threwOut, true);
/* If tweaking some stack parameter makes this fail, shrink monster apply. */
assertEq(gotIn, true);
