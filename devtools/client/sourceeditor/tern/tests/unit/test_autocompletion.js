/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test that tern autocompletions work.
 */

const tern = require("devtools/client/sourceeditor/tern/tern");
const ecma5 = require("devtools/client/sourceeditor/tern/ecma5");

function run_test() {
  do_test_pending();

  const server = new tern.Server({ defs: [ecma5] });
  const code = "[].";
  const query = { type: "completions", file: "test", end: code.length };
  const files = [{ type: "full", name: "test", text: code }];

  server.request({ query: query, files: files }, (error, response) => {
    Assert.equal(error, null);
    Assert.ok(!!response);
    Assert.ok(Array.isArray(response.completions));
    Assert.ok(response.completions.indexOf("concat") != -1);
    do_test_finished();
  });
}
