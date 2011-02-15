// |jit-test| debug
function main() {
  debugger;
  return "failure";
}

setDebuggerHandler("'success'");
setDebug(true);

assertEq(main(), "success");
