/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.importESModule(
  "resource://testing-common/PromiseTestUtils.sys.mjs"
);
PromiseTestUtils.allowMatchingRejectionsGlobally(/File closed/);

// On debug test machine, it takes about 50s to run the test.
requestLongerTimeout(4);

// Test that DevTools panels are rendered in "rtl" (right-to-left) in the Browser Toolbox.
add_task(async function () {
  await pushPref("intl.l10n.pseudo", "bidi");

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({});

  const dir = await ToolboxTask.spawn(null, async () => {
    /* global gToolbox */
    const inspector = await gToolbox.selectTool("inspector");
    return inspector.panelDoc.dir;
  });
  is(dir, "rtl", "Inspector panel has the expected direction");

  await ToolboxTask.destroy();
});
