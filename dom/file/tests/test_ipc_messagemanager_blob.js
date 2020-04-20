"use strict";

const { ExtensionTestUtils } = ChromeUtils.import(
  "resource://testing-common/ExtensionXPCShellUtils.jsm"
);

ExtensionTestUtils.init(this);

function childFrameScript() {
  addMessageListener("test:ipcClonedMessage", function(message) {
    if (!Blob.isInstance(message.json)) {
      sendAsyncMessage(message.name, message.json);
      return;
    }

    let reader = new FileReader();
    reader.addEventListener("load", function() {
      let response =
        reader.result == "this is a great success!" ? message.json : "error";
      sendAsyncMessage(message.name, response);
    });
    reader.readAsText(message.json);
  });
}

add_task(async function test() {
  let page = await ExtensionTestUtils.loadContentPage("about:blank", {
    remote: true,
  });

  page.loadFrameScript(childFrameScript);

  const blobString = "this is a great success!";

  const messages = [
    "hi!",
    "",
    2,
    -0.04,
    3432987324987239872948732982,
    true,
    false,
    null,
    0,

    // Make sure this one is always last.
    new Blob(["this ", "is ", "a ", "great ", "success!"], {
      type: "text/plain",
    }),
  ];
  let receivedMessageIndex = 0;

  let mm = page.browser.messageManager;
  let done = new Promise(resolve => {
    mm.addMessageListener("test:ipcClonedMessage", async message => {
      let data = message.json;

      if (Blob.isInstance(data)) {
        equal(receivedMessageIndex, messages.length - 1, "Blob is last");
        equal(
          data.size,
          messages[receivedMessageIndex].size,
          "Correct blob size"
        );
        equal(
          data.type,
          messages[receivedMessageIndex].type,
          "Correct blob type"
        );

        let reader1 = new FileReader();
        reader1.readAsText(data);

        let reader2 = new FileReader();
        reader2.readAsText(messages[receivedMessageIndex]);

        await Promise.all([
          new Promise(res => (reader1.onload = res)),
          new Promise(res => (reader2.onload = res)),
        ]);

        equal(reader1.result, blobString, "Result 1");
        equal(reader2.result, blobString, "Result 2");

        resolve();
      } else {
        equal(
          data,
          messages[receivedMessageIndex++],
          "Got correct round-tripped response"
        );
      }
    });
  });

  for (let message of messages) {
    mm.sendAsyncMessage("test:ipcClonedMessage", message);
  }

  await page.close();
});
