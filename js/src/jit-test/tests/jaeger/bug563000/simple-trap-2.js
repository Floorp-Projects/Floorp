// |jit-test| debug
setDebug(true);
var x = "notset";
function main() { x = "failure"; }
function success() { x = "success"; }

/* The JSOP_STOP in main. */
trap(main, 10, "success()");
main();

assertEq(x, "success");
