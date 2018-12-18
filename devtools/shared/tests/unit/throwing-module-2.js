"use strict";

function throwingMethod() {
  // eslint-disable-next-line no-throw-literal
  throw "my-exception";
}

throwingMethod();
