/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

function createLinuxMoveTests(aFirstValue, aSecondValue, aMsg) {
  for (let prop of ["screenX", "screenY"]) {
    let first = {};
    first[prop] = aFirstValue;
    let second = {};
    second[prop] = aSecondValue;
    new ResizeMoveTest(
      [first, second],
      /* aInstant */ true,
      `${aMsg} ${prop},${prop}`
    );
    new ResizeMoveTest(
      [first, second],
      /* aInstant */ false,
      `${aMsg} ${prop},${prop}`
    );
  }
}

if (AppConstants.platform == "linux" && gfxInfo.windowProtocol == "wayland") {
  add_task(async () => {
    let tab = await ResizeMoveTest.GetOrCreateTab();
    let browsingContext =
      await ResizeMoveTest.GetOrCreatePopupBrowsingContext();
    let win = browsingContext.topChromeWindow;
    let targetX = win.screenX + 10;
    win.moveTo(targetX, win.screenY);
    await BrowserTestUtils.waitForCondition(() => win.screenX == targetX).catch(
      () => {}
    );
    todo(win.screenX == targetX, "Moving windows on wayland.");
    win.close();
    await BrowserTestUtils.removeTab(tab);
  });
} else {
  createLinuxMoveTests(9, 10, "Move");
  createLinuxMoveTests(10, 0, "Move revert");
  createLinuxMoveTests(10, 10, "Move repeat");

  new ResizeMoveTest(
    [{ screenX: 10 }, { screenY: 10 }, { screenX: 20 }],
    /* aInstant */ true,
    "Move sequence",
    /* aWaitForCompletion */ true
  );

  new ResizeMoveTest(
    [{ screenX: 10 }, { screenY: 10 }, { screenX: 20 }],
    /* aInstant */ false,
    "Move sequence",
    /* aWaitForCompletion */ true
  );
}
