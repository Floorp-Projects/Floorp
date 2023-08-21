/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function () {
  const tab = await addTab(
    "https://example.com/document-builder.sjs?html=test"
  );

  info("Enable F12 and check that devtools open");
  await pushPref("devtools.f12_enabled", true);
  await assertToolboxOpens(tab, { shouldOpen: true });
  await assertToolboxCloses(tab, { shouldClose: true });

  info("Disable F12 and check that devtools will not open");
  await pushPref("devtools.f12_enabled", false);
  await assertToolboxOpens(tab, { shouldOpen: false });

  info("Enable F12 again and open devtools");
  await pushPref("devtools.f12_enabled", true);
  await assertToolboxOpens(tab, { shouldOpen: true });

  info("Disable F12 and check F12 no longer closes devtools");
  await pushPref("devtools.f12_enabled", false);
  await assertToolboxCloses(tab, { shouldClose: false });

  info("Enable F12 and close devtools");
  await pushPref("devtools.f12_enabled", true);
  await assertToolboxCloses(tab, { shouldClose: true });

  info("Disable F12 and check other shortcuts still work");
  await pushPref("devtools.f12_enabled", false);
  const isMac = Services.appinfo.OS == "Darwin";
  const shortcut = {
    key: "i",
    options: { accelKey: true, altKey: isMac, shiftKey: !isMac },
  };
  await assertToolboxOpens(tab, { shouldOpen: true, shortcut });
  // Check F12 still doesn't close the toolbox
  await assertToolboxCloses(tab, { shouldClose: false });
  await assertToolboxCloses(tab, { shouldClose: true, shortcut });

  gBrowser.removeTab(tab);
});

const assertToolboxCloses = async function (tab, { shortcut, shouldClose }) {
  info(
    `Use ${
      shortcut ? "shortcut" : "F12"
    } to close the toolbox (close expected: ${shouldClose})`
  );
  const onToolboxDestroy = gDevTools.once("toolbox-destroyed");

  if (shortcut) {
    EventUtils.synthesizeKey(shortcut.key, shortcut.options);
  } else {
    EventUtils.synthesizeKey("VK_F12", {});
  }

  if (shouldClose) {
    await onToolboxDestroy;
  } else {
    const onTimeout = wait(1000).then(() => "TIMEOUT");
    const res = await Promise.race([onTimeout, onToolboxDestroy]);
    is(res, "TIMEOUT", "No toolbox-destroyed event received");
  }
  is(
    !gDevTools.getToolboxForTab(tab),
    shouldClose,
    `Toolbox was ${shouldClose ? "" : "not "}closed for the test tab`
  );
};

const assertToolboxOpens = async function (tab, { shortcut, shouldOpen }) {
  info(
    `Use ${
      shortcut ? "shortcut" : "F12"
    } to open the toolbox (open expected: ${shouldOpen})`
  );
  const onToolboxReady = gDevTools.once("toolbox-ready");

  if (shortcut) {
    EventUtils.synthesizeKey(shortcut.key, shortcut.options);
  } else {
    EventUtils.synthesizeKey("VK_F12", {});
  }

  if (shouldOpen) {
    await onToolboxReady;
    info(`Received toolbox-ready`);
  } else {
    const onTimeout = wait(1000).then(() => "TIMEOUT");
    const res = await Promise.race([onTimeout, onToolboxReady]);
    is(res, "TIMEOUT", "No toolbox-ready event received");
  }
  is(
    !!gDevTools.getToolboxForTab(tab),
    shouldOpen,
    `Toolbox was ${shouldOpen ? "" : "not "}opened for the test tab`
  );
};
