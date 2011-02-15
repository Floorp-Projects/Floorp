// |jit-test| debug
setDebug(true);
x = "notset";
function myparent(nested) {
  if (nested) {
    /* myparent call in myparent. */
    trap(myparent, 39, "failure()");
  } else {
    x = "success";   
    myparent(true);
  }
}
function failure() { x = "failure"; }

myparent(false);
assertEq(x, "success");
