/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

function test_runner(test) {
  let testTask = async () => {
    // Before each
    pktUI.initPrefs();
    const sandbox = sinon.createSandbox();
    try {
      await test({ sandbox });
    } finally {
      // After each
      sandbox.restore();
    }
  };

  // Copy the name of the test function to identify the test
  Object.defineProperty(testTask, "name", { value: test.name });
  add_task(testTask);
}

test_runner(async function test_pktUI_getAndShowRecsForItem_on({ sandbox }) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.onSaveRecs", true]],
  });
  pktUI.initPrefs();
  const getRecsForItemStub = sandbox.stub(pktApi, "getRecsForItem");

  pktUI.getAndShowRecsForItem(
    {
      resolved_id: "1234",
    },
    {
      success() {},
    }
  );

  Assert.ok(getRecsForItemStub.calledOnce);
  Assert.equal(getRecsForItemStub.getCall(0).args[0], "1234");
});

test_runner(async function test_pktUI_getAndShowRecsForItem_off({ sandbox }) {
  await SpecialPowers.pushPrefEnv({
    set: [["extensions.pocket.onSaveRecs", false]],
  });
  pktUI.initPrefs();
  const getRecsForItemStub = sandbox.stub(pktApi, "getRecsForItem");

  pktUI.getAndShowRecsForItem(
    {
      resolved_id: "1234",
    },
    {
      success() {},
    }
  );

  Assert.ok(getRecsForItemStub.notCalled);
});

test_runner(async function test_pktUI_getAndShowRecsForItem_locale({
  sandbox,
}) {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["extensions.pocket.onSaveRecs", true],
      ["extensions.pocket.onSaveRecs.locales", "de"],
    ],
  });
  pktUI.initPrefs();
  const getRecsForItemStub = sandbox.stub(pktApi, "getRecsForItem");

  pktUI.getAndShowRecsForItem(
    {
      resolved_id: "1234",
    },
    {
      success() {},
    }
  );

  Assert.ok(getRecsForItemStub.notCalled);
});

test_runner(async function test_pktUI_showPanel({ sandbox }) {
  const testFrame = {
    setAttribute: sandbox.stub(),
    style: { width: 0, height: 0 },
  };
  pktUI.setToolbarPanelFrame(testFrame);

  pktUI.showPanel("about:pocket-saved", `saved`);

  Assert.deepEqual(testFrame.setAttribute.args[0], [
    "src",
    `about:pocket-saved?utmSource=firefox_pocket_save_button&locale=${SpecialPowers.Services.locale.appLocaleAsBCP47}`,
  ]);
  Assert.deepEqual(testFrame.style, { width: "350px", height: "110px" });
});
