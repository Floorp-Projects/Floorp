// |jit-test| debug
setDebug(true);
var x = "notset";
function main() { x = "success"; }
function failure() { x = "failure"; }

/* The JSOP_STOP in main. */
trap(main, 10, "failure()");
untrap(main, 10);
main();

assertEq(x, "success");
