/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// There are shutdown issues for which multiple rejections are left uncaught.
// See bug 1018184 for resolving these issues.
const { PromiseTestUtils } = ChromeUtils.import(
  "resource://testing-common/PromiseTestUtils.jsm"
);
PromiseTestUtils.whitelistRejectionsGlobally(/File closed/);

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

  await ToolboxTask.destroy();
});
