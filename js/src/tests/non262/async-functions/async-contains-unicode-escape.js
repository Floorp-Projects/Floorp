var BUGNUMBER = 1315815;
var summary = "async/await containing escapes";

print(BUGNUMBER + ": " + summary);

// Using "eval" as the argument name is fugly, but it means evals below are
// *direct* evals, and so their effects in the unescaped case won't extend
// past each separate |test| call (as would happen if we used a different name
// that made each eval into an indirect eval, affecting code in the global
// scope).
function test(code, eval)
{
  var unescaped = code.replace("###", "async");
  var escaped = code.replace("###", "\\u0061");

  assertThrowsInstanceOf(() => eval(escaped), SyntaxError);
  eval(unescaped);
}

test("### function f() {}", eval);
test("var x = ### function f() {}", eval);
test("### x => {};", eval);
test("var x = ### x => {}", eval);
test("### () => {};", eval);
test("var x = ### () => {}", eval);
test("### (y) => {};", eval);
test("var x = ### (y) => {}", eval);
test("({ ### x() {} })", eval);
test("var x = ### function f() {}", eval);

if (typeof parseModule === "function")
  test("export default ### function f() {}", parseModule);

assertThrowsInstanceOf(() => eval("async await => 1;"),
                       SyntaxError);
assertThrowsInstanceOf(() => eval("async aw\\u0061it => 1;"),
                       SyntaxError);

var async = 0;
assertEq(\u0061sync, 0);

var obj = { \u0061sync() { return 1; } };
assertEq(obj.async(), 1);

async = function() { return 42; };

var z = async(obj);
assertEq(z, 42);

var w = async(obj)=>{};
assertEq(typeof w, "function");

if (typeof reportCompare === "function")
    reportCompare(true, true);
