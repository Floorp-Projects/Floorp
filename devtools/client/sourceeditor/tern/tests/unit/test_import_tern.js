/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that we can require tern.
 */

function run_test() {
  const tern = require("devtools/client/sourceeditor/tern/tern");
  const ecma5 = require("devtools/client/sourceeditor/tern/ecma5");
  const browser = require("devtools/client/sourceeditor/tern/browser");
  Assert.ok(!!tern);
  Assert.ok(!!ecma5);
  Assert.ok(!!browser);
  Assert.equal(typeof tern.Server, "function");
}
