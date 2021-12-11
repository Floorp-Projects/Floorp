"use strict";

/**
 * Test that the name of a sandbox contains the name of all principals.
 */
function test_sandbox_name() {
  let names = [
    "http://example.com/?" + Math.random(),
    "http://example.org/?" + Math.random()
  ];
  let sandbox = Cu.Sandbox(names);
  let fileName = Cu.evalInSandbox(
    "(new Error()).fileName",
    sandbox,
    "latest" /*js version*/,
    ""/*file name*/
  );

  for (let name of names) {
    Assert.ok(fileName.includes(name), `Name ${name} appears in ${fileName}`);
  }
};

function run_test() {
  test_sandbox_name();
}
