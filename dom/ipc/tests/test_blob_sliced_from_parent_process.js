"use strict";

const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

function childFrameScript() {
  /* eslint-env mozilla/frame-script */
  const messageName = "test:blob-slice-test";
  const blobData = ["So", " ", "many", " ", "blobs!"];
  const blobText = blobData.join("");
  const blobType = "text/plain";

  const sliceText = "an";

  function info(msg) {
    sendAsyncMessage(messageName, { op: "info", msg });
  }

  function ok(condition, name, diag) {
    sendAsyncMessage(messageName, { op: "ok", condition, name, diag });
  }

  function is(a, b, name) {
    let pass = a == b;
    let diag = pass ? "" : "got " + a + ", expected " + b;
    ok(pass, name, diag);
  }

  function finish(result) {
    sendAsyncMessage(messageName, { op: "done", result });
  }

  function grabAndContinue(arg) {
    testGenerator.next(arg);
  }

  function* testSteps() {
    addMessageListener(messageName, grabAndContinue);
    let message = yield undefined;

    let blob = message.data;

    ok(Blob.isInstance(blob), "Received a Blob");
    is(blob.size, blobText.length, "Blob has correct length");
    is(blob.type, blobType, "Blob has correct type");

    info("Reading blob");

    let reader = new FileReader();
    reader.addEventListener("load", grabAndContinue);
    reader.readAsText(blob);

    yield undefined;

    is(reader.result, blobText, "Blob has correct data");

    let firstSliceStart = blobData[0].length + blobData[1].length;
    let firstSliceEnd = firstSliceStart + blobData[2].length;

    let slice = blob.slice(firstSliceStart, firstSliceEnd, blobType);

    ok(Blob.isInstance(slice), "Slice returned a Blob");
    is(slice.size, blobData[2].length, "Slice has correct length");
    is(slice.type, blobType, "Slice has correct type");

    info("Reading slice");

    reader = new FileReader();
    reader.addEventListener("load", grabAndContinue);
    reader.readAsText(slice);

    yield undefined;

    is(reader.result, blobData[2], "Slice has correct data");

    let secondSliceStart = blobData[2].indexOf("a");
    let secondSliceEnd = secondSliceStart + sliceText.length;

    slice = slice.slice(secondSliceStart, secondSliceEnd, blobType);

    ok(Blob.isInstance(slice), "Second slice returned a Blob");
    is(slice.size, sliceText.length, "Second slice has correct length");
    is(slice.type, blobType, "Second slice has correct type");

    info("Sending second slice");
    finish(slice);
  }

  let testGenerator = testSteps();
  testGenerator.next();
}

add_task(async function test() {
  let page = await XPCShellContentUtils.loadContentPage(
    "data:text/html,<!DOCTYPE HTML><html><body></body></html>",
    {
      remote: true,
    }
  );

  page.loadFrameScript(childFrameScript);

  const messageName = "test:blob-slice-test";
  const blobData = ["So", " ", "many", " ", "blobs!"];
  const blobType = "text/plain";

  const sliceText = "an";

  await new Promise(resolve => {
    function grabAndContinue(arg) {
      testGenerator.next(arg);
    }

    function* testSteps() {
      let slice = yield undefined;

      ok(Blob.isInstance(slice), "Received a Blob");
      equal(slice.size, sliceText.length, "Slice has correct size");
      equal(slice.type, blobType, "Slice has correct type");

      let reader = new FileReader();
      reader.onload = grabAndContinue;
      reader.readAsText(slice);
      yield undefined;

      equal(reader.result, sliceText, "Slice has correct data");
      resolve();
    }

    let testGenerator = testSteps();
    testGenerator.next();

    let mm = page.browser.messageManager;
    mm.addMessageListener(messageName, function(message) {
      let data = message.data;
      switch (data.op) {
        case "info": {
          info(data.msg);
          break;
        }

        case "ok": {
          ok(data.condition, data.name + " - " + data.diag);
          break;
        }

        case "done": {
          testGenerator.next(data.result);
          break;
        }

        default: {
          ok(false, "Unknown op: " + data.op);
          resolve();
        }
      }
    });

    let blob = new Blob(blobData, { type: blobType });
    mm.sendAsyncMessage(messageName, blob);
  });

  await page.close();
});
