/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test_runner(test) {
  let testTask = async () => {
    // Before each
    const sandbox = sinon.createSandbox();
    try {
      await test({ sandbox });
    } finally {
      // After each
      sandbox.restore();
    }
  };

  // Copy the name of the test function to identify the test
  Object.defineProperty(testTask, "name", { value: test.name });
  add_task(testTask);
}

test_runner(async function test_pktpi_getRecsForItem({ sandbox }) {
  const apiRequestStub = sandbox.stub(pktApi, "apiRequest");

  pktApi.getRecsForItem("1234", {
    success() {},
  });

  Assert.ok(apiRequestStub.calledOnce);
  Assert.equal(apiRequestStub.getCall(0).args[0].data.item_id, "1234");
});
