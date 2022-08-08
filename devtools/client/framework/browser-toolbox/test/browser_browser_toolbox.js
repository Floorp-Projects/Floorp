/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

add_task(async function() {
  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({});

  const hasCloseButton = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    return !!gToolbox.doc.getElementById("toolbox-close");
  });
  ok(!hasCloseButton, "Browser toolbox doesn't have a close button");

  info("Trigger F5 key shortcut and ensure nothing happens");
  info(
    "If F5 triggers a full reload, the mochitest will stop here as firefox instance will be restarted"
  );
  const previousInnerWindowId =
    window.browsingContext.currentWindowGlobal.innerWindowId;
  function onUnload() {
    ok(false, "The top level window shouldn't be reloaded/closed");
  }
  window.addEventListener("unload", onUnload);
  await ToolboxTask.spawn(null, async () => {
    const isMacOS = Services.appinfo.OS === "Darwin";
    const { win } = gToolbox;
    // Simulate CmdOrCtrl+R
    win.dispatchEvent(
      new win.KeyboardEvent("keydown", {
        bubbles: true,
        ctrlKey: !isMacOS,
        metaKey: isMacOS,
        keyCode: "r".charCodeAt(0),
      })
    );
    // Simulate F5
    win.dispatchEvent(
      new win.KeyboardEvent("keydown", {
        bubbles: true,
        keyCode: win.KeyEvent.DOM_VK_F5,
      })
    );
  });

  // Let a chance to trigger the regression where the top level document closes or reloads
  await wait(1000);

  is(
    window.browsingContext.currentWindowGlobal.innerWindowId,
    previousInnerWindowId,
    "Check the browser.xhtml wasn't reloaded when pressing F5"
  );
  window.removeEventListener("unload", onUnload);

  await ToolboxTask.destroy();
});
