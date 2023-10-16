/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function secondCall() {
  // This comment is useful: ☺
  debugger;
  function foo() {}
  if (x) {
    foo();
  }
  return 44;
}

var x = true;
console.log("hi")
