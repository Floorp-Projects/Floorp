/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Bug 1328293
self.onfetch = function(event) {
  let a = 5;
  console.log(a);
};
