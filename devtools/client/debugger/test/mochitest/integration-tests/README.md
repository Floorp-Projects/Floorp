# Integration tests folder

This folder contains all the sub tests ran by the integration tests.
The integration tests are in the parent folder and are named `browser_dbg-integration-*.js`.
Each time you execute one of the integration tests, all the sub tests implemented in this folder will be ran.
Sub tests will be ran in a precise order, this is why the module names are prefixed by a number.

Sub tests are javascript files which have all typical test helpers available in their scope:
* devtools/client/shared/test/shared-head.js
* devtools/client/debugger/test/mochitest/shared-head.js

They should call the `addIntegrationTask()` method to register a new sub test:
```
addIntegrationTask(async function testMyNewIntegrationSubTest(
  testServer,
  testUrl,
  { isCompressed }
) {
  info("My new integration sub test");

  // `testServer` is a reference to the fake http server returned by `createVersionizedHttpTestServer()`.
  // This can be useful to better control the actual content delivered by the server.

  // `testUrl` is the URL of the test page the the debugger currently inspects. 

  // The third argument is the environment object, that helps know which particular integration test currently runs.
  // You may have different assertions based on the actual test that runs.
});
```
