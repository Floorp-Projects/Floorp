// |jit-test| debug
var result = "unset";
function main() {
  result = "failure";
  debugger;
}
function nop() { }

setDebuggerHandler("result = 'success'; nop()");
setDebug(true);

main();
assertEq(result, "success");
