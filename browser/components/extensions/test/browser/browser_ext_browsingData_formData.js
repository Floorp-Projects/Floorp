/* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set sts=2 sw=2 et tw=80: */
"use strict";

ChromeUtils.defineESModuleGetters(this, {
  FormHistory: "resource://gre/modules/FormHistory.sys.mjs",
  PlacesUtils: "resource://gre/modules/PlacesUtils.sys.mjs",
});

const REFERENCE_DATE = Date.now();

async function countEntries(fieldname, message, expected) {
  let count = await FormHistory.count({ fieldname });
  is(count, expected, message);
}

async function setupFormHistory() {
  async function searchFirstEntry(terms, params) {
    return (await FormHistory.search(terms, params))[0];
  }

  // Make sure we've got a clean DB to start with, then add the entries we'll be testing.
  await FormHistory.update([
    { op: "remove" },
    {
      op: "add",
      fieldname: "reference",
      value: "reference",
    },
    {
      op: "add",
      fieldname: "10secondsAgo",
      value: "10s",
    },
    {
      op: "add",
      fieldname: "10minutesAgo",
      value: "10m",
    },
  ]);

  // Age the entries to the proper vintage.
  let timestamp = PlacesUtils.toPRTime(REFERENCE_DATE);
  let result = await searchFirstEntry(["guid"], { fieldname: "reference" });
  await FormHistory.update({
    op: "update",
    firstUsed: timestamp,
    guid: result.guid,
  });

  timestamp = PlacesUtils.toPRTime(REFERENCE_DATE - 10000);
  result = await searchFirstEntry(["guid"], { fieldname: "10secondsAgo" });
  await FormHistory.update({
    op: "update",
    firstUsed: timestamp,
    guid: result.guid,
  });

  timestamp = PlacesUtils.toPRTime(REFERENCE_DATE - 10000 * 60);
  result = await searchFirstEntry(["guid"], { fieldname: "10minutesAgo" });
  await FormHistory.update({
    op: "update",
    firstUsed: timestamp,
    guid: result.guid,
  });

  // Sanity check.
  await countEntries(
    "reference",
    "Checking for 10minutes form history entry creation",
    1
  );
  await countEntries(
    "10secondsAgo",
    "Checking for 1hour form history entry creation",
    1
  );
  await countEntries(
    "10minutesAgo",
    "Checking for 1hour10minutes form history entry creation",
    1
  );
}

add_task(async function testFormData() {
  function background() {
    browser.test.onMessage.addListener(async (msg, options) => {
      if (msg == "removeFormData") {
        await browser.browsingData.removeFormData(options);
      } else {
        await browser.browsingData.remove(options, { formData: true });
      }
      browser.test.sendMessage("formDataRemoved");
    });
  }

  let extension = ExtensionTestUtils.loadExtension({
    background,
    manifest: {
      permissions: ["browsingData"],
    },
  });

  async function testRemovalMethod(method) {
    // Clear form data with no since value.
    await setupFormHistory();
    extension.sendMessage(method, {});
    await extension.awaitMessage("formDataRemoved");

    await countEntries(
      "reference",
      "reference form entry should be deleted.",
      0
    );
    await countEntries(
      "10secondsAgo",
      "10secondsAgo form entry should be deleted.",
      0
    );
    await countEntries(
      "10minutesAgo",
      "10minutesAgo form entry should be deleted.",
      0
    );

    // Clear form data with recent since value.
    await setupFormHistory();
    extension.sendMessage(method, { since: REFERENCE_DATE });
    await extension.awaitMessage("formDataRemoved");

    await countEntries(
      "reference",
      "reference form entry should be deleted.",
      0
    );
    await countEntries(
      "10secondsAgo",
      "10secondsAgo form entry should still exist.",
      1
    );
    await countEntries(
      "10minutesAgo",
      "10minutesAgo form entry should still exist.",
      1
    );

    // Clear form data with old since value.
    await setupFormHistory();
    extension.sendMessage(method, { since: REFERENCE_DATE - 1000000 });
    await extension.awaitMessage("formDataRemoved");

    await countEntries(
      "reference",
      "reference form entry should be deleted.",
      0
    );
    await countEntries(
      "10secondsAgo",
      "10secondsAgo form entry should be deleted.",
      0
    );
    await countEntries(
      "10minutesAgo",
      "10minutesAgo form entry should be deleted.",
      0
    );
  }

  await extension.startup();

  await testRemovalMethod("removeFormData");
  await testRemovalMethod("remove");

  await extension.unload();
});
