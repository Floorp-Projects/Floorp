/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doSearchEngineDefaultIdTest({ trigger, assert }) {
  await doTest(async browser => {
    info("Test with current engine");
    const defaultEngine = await Services.search.getDefault();

    await openPopup("x");
    await trigger();
    await assert(defaultEngine.telemetryId);
  });

  await doTest(async browser => {
    info("Test with new engine");
    const defaultEngine = await Services.search.getDefault();
    const newEngineName = "NewDummyEngine";
    await SearchTestUtils.installSearchExtension({
      name: newEngineName,
      search_url: "https://example.com/",
      search_url_get_params: "q={searchTerms}",
    });
    const newEngine = await Services.search.getEngineByName(newEngineName);
    Assert.notEqual(defaultEngine.telemetryId, newEngine.telemetryId);
    await Services.search.setDefault(
      newEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );

    await openPopup("x");
    await trigger();
    await assert(newEngine.telemetryId);

    await Services.search.setDefault(
      defaultEngine,
      Ci.nsISearchService.CHANGE_REASON_UNKNOWN
    );
  });
}
