// ================================================
// Load mocking/stubbing library, sinon
// docs: http://sinonjs.org/releases/v2.3.2/
ChromeUtils.import("resource://gre/modules/Timer.jsm");
Services.scriptloader.loadSubScript("resource://testing-common/sinon-2.3.2.js", this);
/* globals sinon */
// ================================================

/* eslint-disable mozilla/use-chromeutils-generateqi */

add_task(async function test_no_result_node() {
  let functionSpy = sinon.stub().returns(Promise.resolve());

  await PlacesUIUtils.batchUpdatesForNode(null, 1, functionSpy);

  Assert.ok(functionSpy.calledOnce,
    "Passing a null result node should still call the wrapped function");
});

add_task(async function test_under_batch_threshold() {
  let functionSpy = sinon.stub().returns(Promise.resolve());
  let resultNode = {
    QueryInterface() {
      return this;
    },
    onBeginUpdateBatch: sinon.spy(),
    onEndUpdateBatch: sinon.spy(),
  };

  await PlacesUIUtils.batchUpdatesForNode(resultNode, 1, functionSpy);

  Assert.ok(functionSpy.calledOnce,
    "Wrapped function should be called once");
  Assert.ok(resultNode.onBeginUpdateBatch.notCalled,
    "onBeginUpdateBatch should not have been called");
  Assert.ok(resultNode.onEndUpdateBatch.notCalled,
    "onEndUpdateBatch should not have been called");
});

add_task(async function test_over_batch_threshold() {
  let functionSpy = sinon.stub().callsFake(() => {
    Assert.ok(resultNode.onBeginUpdateBatch.calledOnce,
      "onBeginUpdateBatch should have been called before the function");
    Assert.ok(resultNode.onEndUpdateBatch.notCalled,
      "onEndUpdateBatch should not have been called before the function");

    return Promise.resolve();
  });
  let resultNode = {
    QueryInterface() {
      return this;
    },
    onBeginUpdateBatch: sinon.spy(),
    onEndUpdateBatch: sinon.spy(),
  };

  await PlacesUIUtils.batchUpdatesForNode(resultNode, 100, functionSpy);

  Assert.ok(functionSpy.calledOnce,
    "Wrapped function should be called once");
  Assert.ok(resultNode.onBeginUpdateBatch.calledOnce,
    "onBeginUpdateBatch should have been called");
  Assert.ok(resultNode.onEndUpdateBatch.calledOnce,
    "onEndUpdateBatch should have been called");
});

add_task(async function test_wrapped_function_throws() {
  let error = new Error("Failed!");
  let functionSpy = sinon.stub().throws(error);
  let resultNode = {
    QueryInterface() {
      return this;
    },
    onBeginUpdateBatch: sinon.spy(),
    onEndUpdateBatch: sinon.spy(),
  };

  let raisedError;
  try {
    await PlacesUIUtils.batchUpdatesForNode(resultNode, 100, functionSpy);
  } catch (ex) {
    raisedError = ex;
  }

  Assert.ok(functionSpy.calledOnce,
    "Wrapped function should be called once");
  Assert.ok(resultNode.onBeginUpdateBatch.calledOnce,
    "onBeginUpdateBatch should have been called");
  Assert.ok(resultNode.onEndUpdateBatch.calledOnce,
    "onEndUpdateBatch should have been called");
  Assert.equal(raisedError, error,
    "batchUpdatesForNode should have raised the error from the wrapped function");
});
