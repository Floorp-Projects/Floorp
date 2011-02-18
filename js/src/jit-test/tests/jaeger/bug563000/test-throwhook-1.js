// |jit-test| debug
var result1 = "unset";
var result2 = "failure";
function main() {
  result1 = "failure";
  try {
    throw "something";
  } catch(e) {
    result2 = "success";
  }
}
function nop() { }

setDebug(true);
setThrowHook("result1 = 'success'; nop()");

main();
assertEq(result1, "success");
assertEq(result2, "success");
