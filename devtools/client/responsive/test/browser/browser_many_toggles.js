/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify RDM can be toggled many times in a raw

const TEST_URL = "https://example.com/";

addRDMTask(
  null,
  async function () {
    const tab = await addTab(TEST_URL);

    // Record all currently active RDMs, there may not be one created on each loop
    let opened = 0;
    ResponsiveUIManager.on("on", () => opened++);
    ResponsiveUIManager.on("off", () => opened--);

    for (let i = 0; i < 10; i++) {
      info(`Toggling RDM #${i + 1}`);
      // This may throw when we were just closing is still ongoing,
      // ignore any exception.
      openRDM(tab).catch(e => {});
      // Sometime pause in order to cover both full synchronous opening and close
      // but also the same but with some pause between each operation.
      if (i % 2 == 0) {
        info("Wait a bit after open");
        await wait(250);
      }
      closeRDM(tab);
      if (i % 3 == 0) {
        info("Wait a bit after close");
        await wait(250);
      }
    }

    // Wait a bit so that we can receive the very last ResponsiveUIManager `on` event,
    // and properly wait for its closing.
    await wait(1000);

    // This is important to wait for all destruction as closing the tab while RDM
    // is still closing may lead to exception because of pending cleanup RDP requests.
    info("Wait for all opened RDM to be closed before closing the tab");
    await waitFor(() => opened == 0);
    info("All RDM are closed");

    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);
