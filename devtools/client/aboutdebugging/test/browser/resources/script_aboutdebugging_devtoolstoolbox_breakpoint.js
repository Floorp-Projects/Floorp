/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Random method on which a breakpoint will be set from the DevTools UI in the
// test.
window.testMethod = function() {
  const a = 1;
  const b = 2;
  return a + b;
};
