"use strict";

async function installAndStartExtension() {
  function contentScript() {
    window.addEventListener("keydown", aEvent => {
      console.log("keydown event is fired");
      if (aEvent.defaultPrevented) {
        return;
      }
      let selection = window.getSelection();
      if (selection.isCollapsed) {
        return;
      }
      if (aEvent.ctrlKey && aEvent.key === "k") {
        document.execCommand("createLink", false, "http://example.com/");
        aEvent.preventDefault();
      }
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    manifest: {
      content_scripts: [
        {
          js: ["content_script.js"],
          matches: ["<all_urls>"],
          run_at: "document_start",
        },
      ],
    },
    files: {
      "content_script.js": contentScript,
    },
  });

  await extension.startup();

  return extension;
}

add_task(async function() {
  await pushPrefs(["dom.input_events.beforeinput.enabled", true]);

  const extension = await installAndStartExtension();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/dom/events/test/file_beforeinput_by_execCommand_in_contentscript.html",
    true
  );

  function runTest() {
    var editor = content.document.querySelector("[contenteditable]");
    editor.focus();
    content.document.getSelection().selectAllChildren(editor);
    var beforeinput;
    editor.addEventListener("beforeinput", aEvent => {
      beforeinput = aEvent;
    });
    editor.addEventListener("input", aEvent => {
      if (!beforeinput) {
        sendAsyncMessage("Test:BeforeInputInContentEditable", {
          succeeded: false,
          message: "No beforeinput event is fired",
        });
        return;
      }
      sendAsyncMessage("Test:BeforeInputInContentEditable", {
        succeeded:
          editor.innerHTML === '<a href="http://example.com/">abcdef</a>',
        message: `editor.innerHTML=${editor.innerHTML}`,
      });
    });
  }

  try {
    tab.linkedBrowser.messageManager.loadFrameScript(
      "data:,(" + runTest.toString() + ")();",
      false
    );

    let received = false;
    let testResult = new Promise(resolve => {
      let mm = tab.linkedBrowser.messageManager;
      mm.addMessageListener(
        "Test:BeforeInputInContentEditable",
        function onFinish(aMsg) {
          mm.removeMessageListener(
            "Test:BeforeInputInContentEditable",
            onFinish
          );
          is(aMsg.data.succeeded, true, aMsg.data.message);
          resolve();
        }
      );
    });
    info("Sending Ctrl+K...");
    await BrowserTestUtils.synthesizeKey(
      "k",
      { ctrlKey: true },
      tab.linkedBrowser
    );
    info("Waiting test result...");
    await testResult;
  } finally {
    BrowserTestUtils.removeTab(tab);
    await extension.unload();
  }
});
