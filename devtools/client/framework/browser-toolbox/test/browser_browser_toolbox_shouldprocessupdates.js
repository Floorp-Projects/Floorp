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

add_task(async function () {
  // Running devtools should prevent processing updates.  By setting this
  // environment variable and then inspecting it from the launched devtools
  // process, we can witness update processing being skipped.
  Services.env.set("MOZ_TEST_PROCESS_UPDATES", "1");

  const ToolboxTask = await initBrowserToolboxTask();
  await ToolboxTask.importFunctions({});

  let result = await ToolboxTask.spawn(null, async () => {
    const result = {
      exists: Services.env.exists("MOZ_TEST_PROCESS_UPDATES"),
      get: Services.env.get("MOZ_TEST_PROCESS_UPDATES"),
    };
    // Log so that we have a hope of debugging.
    console.log("result", result);
    return JSON.stringify(result);
  });

  result = JSON.parse(result);
  ok(result.exists, "MOZ_TEST_PROCESS_UPDATES exists in subprocess");
  is(
    result.get,
    "ShouldNotProcessUpdates(): DevToolsLaunching",
    "MOZ_TEST_PROCESS_UPDATES is correct in subprocess"
  );

  await ToolboxTask.destroy();
});
