/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// We don't need any computation in the worker,
// but at least register a fetch listener so that
// we force instantiating the SW when loading the page.
self.onfetch = function(event) {
  // do nothing.
};
