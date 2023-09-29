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
// For testing that anonymous functions do not show up in the
// quick open panel browser_dbg-quick-open.js
() => { }
console.log("hi")
