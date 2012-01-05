// |jit-test| debug
setDebug(true);
x = "notset";

function doNothing() { }

function myparent(nested) {
  if (nested) {
    /* JSOP_CALL to doNothing in myparent with nested = true. */
    trap(myparent, 26, "success()");
    doNothing();
  } else {
    doNothing();
  }
}
/* JSOP_CALL to doNothing in myparent with nested = false. */
trap(myparent, 37, "myparent(true)");

function success() {
  x = "success";
}

myparent(false);
assertEq(x, "success");
