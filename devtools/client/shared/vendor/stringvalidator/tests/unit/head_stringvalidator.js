"use strict";
var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});
const { console } = Cu.import("resource://gre/modules/Console.jsm", {});

this.validator = require("devtools/client/shared/vendor/stringvalidator/validator");

function describe(suite, testFunc) {
  do_print(`\n                            Test suite: ${suite}`.toUpperCase());
  testFunc();
}

function it(description, testFunc) {
  do_print(`\n                              - ${description}:\n`.toUpperCase());
  testFunc();
}
