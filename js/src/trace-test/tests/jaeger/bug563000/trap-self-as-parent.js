setDebug(true);
x = "notset";

function myparent(nested) {
  if (nested) {
    /* noop call in myparent */
    trap(myparent, 48, "success()");
  } else {
    myparent(true);
    x = "failure";
    noop();
  }
}
function noop() { }
function success() { x = "success"; }

myparent();
assertEq(x, "success");
