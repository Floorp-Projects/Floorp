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

add_task(async function () {
  const extension = await installAndStartExtension();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/dom/events/test/file_beforeinput_by_execCommand_in_contentscript.html",
    true
  );

  /**
   * Document.execCommand() shouldn't cause `beforeinput`, but it may be used
   * by addons for emulating user input and make the input undoable on builtin
   * editors.  Therefore, if and only if it's called by addons, `beforeinput`
   * should be fired.
   */
  function runTest() {
    const editor = content.document.querySelector("[contenteditable]");
    editor.focus();
    content.document.getSelection().selectAllChildren(editor);
    let beforeinput;
    editor.addEventListener("beforeinput", aEvent => {
      beforeinput = aEvent;
    });
    const description = 'Test execCommand("createLink")';
    editor.addEventListener("input", aEvent => {
      if (!beforeinput) {
        sendAsyncMessage("Test:BeforeInputInContentEditable", {
          succeeded: false,
          message: `${description}: No beforeinput event is fired`,
        });
        return;
      }
      sendAsyncMessage("Test:BeforeInputInContentEditable", {
        succeeded:
          editor.innerHTML === '<a href="http://example.com/">abcdef</a>',
        message: `${description}: editor.innerHTML=${editor.innerHTML}`,
      });
    });
  }

  try {
    tab.linkedBrowser.messageManager.loadFrameScript(
      "data:,(" + runTest.toString() + ")();",
      false
    );

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

add_task(async function () {
  const extension = await installAndStartExtension();
  const tab = await BrowserTestUtils.openNewForegroundTab(
    gBrowser,
    "http://example.com/browser/dom/events/test/file_beforeinput_by_execCommand_in_contentscript.html",
    true
  );

  /**
   * Document.execCommand() from addons should be treated as a user input.
   * Therefore, it should not block first nested Document.execCommand() call
   * in a "beforeinput" event listener in the web app.
   */
  function runTest() {
    const editor = content.document.querySelectorAll("[contenteditable]")[1];
    editor.focus();
    content.document.getSelection().selectAllChildren(editor);
    const beforeInputs = [];
    editor.parentNode.addEventListener(
      "beforeinput",
      aEvent => {
        beforeInputs.push(aEvent);
      },
      { capture: true }
    );
    const description =
      'Test web app calls execCommand("insertText") on "beforeinput"';
    editor.addEventListener("input", aEvent => {
      if (!beforeInputs.length) {
        sendAsyncMessage("Test:BeforeInputInContentEditable", {
          succeeded: false,
          message: `${description}: No beforeinput event is fired`,
        });
        return;
      }
      if (beforeInputs.length > 1) {
        sendAsyncMessage("Test:BeforeInputInContentEditable", {
          succeeded: false,
          message: `${description}: Too many beforeinput events are fired`,
        });
        return;
      }
      sendAsyncMessage("Test:BeforeInputInContentEditable", {
        succeeded: editor.innerHTML.replace("<br>", "") === "ABCDEF",
        message: `${description}: editor.innerHTML=${editor.innerHTML}`,
      });
    });
  }

  try {
    tab.linkedBrowser.messageManager.loadFrameScript(
      "data:,(" + runTest.toString() + ")();",
      false
    );

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
