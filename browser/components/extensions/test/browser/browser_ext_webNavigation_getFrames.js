/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

add_task(function* testWebNavigationGetNonExistentTab() {
  let extension = ExtensionTestUtils.loadExtension({
    background: async function() {
      // There is no "tabId = 0" because the id assigned by tabTracker (defined in ext-utils.js)
      // starts from 1.
      await browser.test.assertRejects(
        browser.webNavigation.getAllFrames({tabId: 0}),
        "Invalid tab ID: 0",
        "getAllFrames rejected Promise should pass the expected error");

      // There is no "tabId = 0" because the id assigned by tabTracker (defined in ext-utils.js)
      // starts from 1, processId is currently marked as optional and it is ignored.
      await browser.test.assertRejects(
        browser.webNavigation.getFrame({tabId: 0, frameId: 15, processId: 20}),
        "Invalid tab ID: 0",
        "getFrame rejected Promise should pass the expected error");

      browser.test.sendMessage("getNonExistentTab.done");
    },
    manifest: {
      permissions: ["webNavigation"],
    },
  });
  info("load complete");

  yield extension.startup();
  info("startup complete");

  yield extension.awaitMessage("getNonExistentTab.done");

  yield extension.unload();
  info("extension unloaded");
});

add_task(function* testWebNavigationFrames() {
  let extension = ExtensionTestUtils.loadExtension({
    background: async function() {
      let tabId;
      let collectedDetails = [];

      browser.webNavigation.onCompleted.addListener(async details => {
        collectedDetails.push(details);

        if (details.frameId !== 0) {
          // wait for the top level iframe to be complete
          return;
        }

        let getAllFramesDetails = await browser.webNavigation.getAllFrames({tabId});

        let getFramePromises = getAllFramesDetails.map(({frameId}) => {
          // processId is currently marked as optional and it is ignored.
          return browser.webNavigation.getFrame({tabId, frameId, processId: 0});
        });

        let getFrameResults = await Promise.all(getFramePromises);
        browser.test.sendMessage("webNavigationFrames.done", {
          collectedDetails, getAllFramesDetails, getFrameResults,
        });

        // Pick a random frameId.
        let nonExistentFrameId = Math.floor(Math.random() * 10000);

        // Increment the picked random nonExistentFrameId until it doesn't exists.
        while (getAllFramesDetails.filter((details) => details.frameId == nonExistentFrameId).length > 0) {
          nonExistentFrameId += 1;
        }

        // Check that getFrame Promise is rejected with the expected error message on nonexistent frameId.
        await browser.test.assertRejects(
          browser.webNavigation.getFrame({tabId, frameId: nonExistentFrameId, processId: 20}),
          `No frame found with frameId: ${nonExistentFrameId}`,
          "getFrame promise should be rejected with the expected error message on unexistent frameId");

        await browser.tabs.remove(tabId);
        browser.test.sendMessage("webNavigationFrames.done");
      });

      let tab = await browser.tabs.create({url: "tab.html"});
      tabId = tab.id;
    },
    manifest: {
      permissions: ["webNavigation", "tabs"],
    },
    files: {
      "tab.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
          <body>
            <iframe src="subframe.html"></iframe>
            <iframe src="subframe.html"></iframe>
          </body>
        </html>
      `,
      "subframe.html": `
        <!DOCTYPE html>
        <html>
          <head>
            <meta charset="utf-8">
          </head>
        </html>
      `,
    },
  });
  info("load complete");

  yield extension.startup();
  info("startup complete");

  let {
    collectedDetails,
    getAllFramesDetails,
    getFrameResults,
  } = yield extension.awaitMessage("webNavigationFrames.done");

  is(getAllFramesDetails.length, 3, "expected number of frames found");
  is(getAllFramesDetails.length, collectedDetails.length,
     "number of frames found should equal the number onCompleted events collected");

  is(getAllFramesDetails[0].frameId, 0, "the root frame has the expected frameId");
  is(getAllFramesDetails[0].parentFrameId, -1, "the root frame has the expected parentFrameId");

  // ordered by frameId
  let sortByFrameId = (el1, el2) => {
    let val1 = el1 ? el1.frameId : -1;
    let val2 = el2 ? el2.frameId : -1;
    return val1 - val2;
  };

  collectedDetails = collectedDetails.sort(sortByFrameId);
  getAllFramesDetails = getAllFramesDetails.sort(sortByFrameId);
  getFrameResults = getFrameResults.sort(sortByFrameId);

  info("check frame details content");

  is(getFrameResults.length, getAllFramesDetails.length,
     "getFrame and getAllFrames should return the same number of results");

  Assert.deepEqual(getFrameResults, getAllFramesDetails,
                   "getFrame and getAllFrames should return the same results");

  info(`check frame details collected and retrieved with getAllFrames`);

  for (let [i, collected] of collectedDetails.entries()) {
    let getAllFramesDetail = getAllFramesDetails[i];

    is(getAllFramesDetail.frameId, collected.frameId, "frameId");
    is(getAllFramesDetail.parentFrameId, collected.parentFrameId, "parentFrameId");
    is(getAllFramesDetail.tabId, collected.tabId, "tabId");

    // This can be uncommented once Bug 1246125 has been fixed
    // is(getAllFramesDetail.url, collected.url, "url");
  }

  info("frame details content checked");

  yield extension.awaitMessage("webNavigationFrames.done");

  yield extension.unload();
  info("extension unloaded");
});
