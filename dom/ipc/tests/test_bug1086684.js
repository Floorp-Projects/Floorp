"use strict";
/* eslint-env mozilla/frame-script */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { XPCShellContentUtils } = ChromeUtils.import(
  "resource://testing-common/XPCShellContentUtils.jsm"
);

XPCShellContentUtils.init(this);

const childFramePath = "/file_bug1086684.html";
const childFrameURL = "http://example.com" + childFramePath;

const childFrameContents = `<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
</head>
<body>
<div id="content">
    <input type="file" id="f">
</div>
</body>
</html>`;

const server = XPCShellContentUtils.createHttpServer({
  hosts: ["example.com"],
});
server.registerPathHandler(childFramePath, (request, response) => {
  response.write(childFrameContents);
});

function childFrameScript() {
  "use strict";

  let { MockFilePicker } = ChromeUtils.import(
    "resource://testing-common/MockFilePicker.jsm"
  );

  function parentReady(message) {
    MockFilePicker.init(content);
    MockFilePicker.setFiles([message.data.file]);
    MockFilePicker.returnValue = MockFilePicker.returnOK;

    let input = content.document.getElementById("f");
    input.addEventListener("change", () => {
      MockFilePicker.cleanup();
      let value = input.value;
      message.target.sendAsyncMessage("testBug1086684:childDone", { value });
    });

    input.focus();
    input.click();
  }

  addMessageListener("testBug1086684:parentReady", function(message) {
    parentReady(message);
  });
}

add_task(async function() {
  Services.prefs.setBoolPref("dom.security.https_first", false);
  let page = await XPCShellContentUtils.loadContentPage(childFrameURL, {
    remote: true,
  });

  page.loadFrameScript(childFrameScript);

  await new Promise(resolve => {
    let test;
    function* testStructure(mm) {
      let value;

      function testDone(msg) {
        test.next(msg.data.value);
      }

      mm.addMessageListener("testBug1086684:childDone", testDone);

      let blob = new Blob([]);
      let file = new File([blob], "helloworld.txt", { type: "text/plain" });

      mm.sendAsyncMessage("testBug1086684:parentReady", { file });
      value = yield;

      // Note that the "helloworld.txt" passed in above doesn't affect the
      // 'value' getter. Because we're mocking a file using a blob, we ask the
      // blob for its path, which is the empty string.
      equal(value, "", "got the right answer and didn't crash");

      resolve();
    }

    test = testStructure(page.browser.messageManager);
    test.next();
  });

  await page.close();
  Services.prefs.clearUserPref("dom.security.https_first");
});
