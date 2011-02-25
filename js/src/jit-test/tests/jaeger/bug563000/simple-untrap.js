// |jit-test| debug
setDebug(true);
var x = "notset";
function main() { x = "success"; }
function failure() { x = "failure"; }

/* The JSOP_STOP in a. */
trap(main, 8, "failure()");
untrap(main, 8);
main();

assertEq(x, "success");
