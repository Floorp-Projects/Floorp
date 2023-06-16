/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const methodToMock = function () {
  return "Original value";
};

exports.methodToMock = methodToMock;
exports.someProperty = "someProperty";
