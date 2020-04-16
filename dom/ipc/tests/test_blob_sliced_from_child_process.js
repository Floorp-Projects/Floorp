"use strict";
/* eslint-env mozilla/frame-script */

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

ExtensionTestUtils.init(this);

function childFrameScript() {
  "use strict";

  const messageName = "test:blob-slice-test";
  const blobData = ["So", " ", "many", " ", "blobs!"];
  const blobType = "text/plain";

  let blob = new Blob(blobData, { type: blobType });

  let firstSliceStart = blobData[0].length + blobData[1].length;
  let firstSliceEnd = firstSliceStart + blobData[2].length;

  let slice = blob.slice(firstSliceStart, firstSliceEnd, blobType);

  let secondSliceStart = blobData[2].indexOf("a");
  let secondSliceEnd = secondSliceStart + 2;

  slice = slice.slice(secondSliceStart, secondSliceEnd, blobType);

  sendAsyncMessage(messageName, { blob });
  sendAsyncMessage(messageName, { slice });
}

add_task(async function test() {
  let page = await ExtensionTestUtils.loadContentPage(
    "data:text/html,<!DOCTYPE HTML><html><body></body></html>",
    {
      remote: true,
    }
  );

  page.loadFrameScript(childFrameScript);

  const messageName = "test:blob-slice-test";
  const blobData = ["So", " ", "many", " ", "blobs!"];
  const blobText = blobData.join("");
  const blobType = "text/plain";

  const sliceText = "an";

  let receivedBlob = false;
  let receivedSlice = false;

  let resolveBlob, resolveSlice;
  let blobPromise = new Promise(resolve => {
    resolveBlob = resolve;
  });
  let slicePromise = new Promise(resolve => {
    resolveSlice = resolve;
  });

  let mm = page.browser.messageManager;
  mm.addMessageListener(messageName, function(message) {
    if ("blob" in message.data) {
      equal(receivedBlob, false, "Have not yet received Blob");
      equal(receivedSlice, false, "Have not yet received Slice");

      receivedBlob = true;

      let blob = message.data.blob;

      ok(Blob.isInstance(blob), "Received a Blob");
      equal(blob.size, blobText.length, "Blob has correct size");
      equal(blob.type, blobType, "Blob has correct type");

      let slice = blob.slice(
        blobText.length - blobData[blobData.length - 1].length,
        blob.size,
        blobType
      );

      ok(Blob.isInstance(slice), "Slice returned a Blob");
      equal(
        slice.size,
        blobData[blobData.length - 1].length,
        "Slice has correct size"
      );
      equal(slice.type, blobType, "Slice has correct type");

      let reader = new FileReader();
      reader.onload = function() {
        equal(
          reader.result,
          blobData[blobData.length - 1],
          "Slice has correct data"
        );

        resolveBlob();
      };
      reader.readAsText(slice);
    } else if ("slice" in message.data) {
      equal(receivedBlob, true, "Already received Blob");
      equal(receivedSlice, false, "Have not yet received Slice");

      receivedSlice = true;

      let slice = message.data.slice;

      ok(Blob.isInstance(slice), "Received a Blob for slice");
      equal(slice.size, sliceText.length, "Slice has correct size");
      equal(slice.type, blobType, "Slice has correct type");

      let reader = new FileReader();
      reader.onload = function() {
        equal(reader.result, sliceText, "Slice has correct data");

        let slice2 = slice.slice(1, 2, blobType);

        ok(Blob.isInstance(slice2), "Slice returned a Blob");
        equal(slice2.size, 1, "Slice has correct size");
        equal(slice2.type, blobType, "Slice has correct type");

        let reader2 = new FileReader();
        reader2.onload = function() {
          equal(reader2.result, sliceText[1], "Slice has correct data");

          resolveSlice();
        };
        reader2.readAsText(slice2);
      };
      reader.readAsText(slice);
    } else {
      ok(false, "Received a bad message: " + JSON.stringify(message.data));
    }
  });

  await blobPromise;
  await slicePromise;
});
