/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Verify that inspector does not reboot when opening and closing RDM.

const TEST_URL = "http://example.com/";

const checkToolbox = async function (tab, location) {
  const toolbox = gDevTools.getToolboxForTab(tab);
  ok(!!toolbox, `Toolbox exists ${location}`);
};

addRDMTask(
  "",
  async function () {
    const tab = await addTab(TEST_URL);

    info("Open toolbox outside RDM");
    {
      const { toolbox, inspector } = await openInspector();
      inspector.walker.once("new-root", () => {
        ok(false, "Inspector saw new root, would reboot!");
      });
      checkToolbox(tab, "outside RDM");
      await openRDM(tab);
      checkToolbox(tab, "after opening RDM");
      await closeRDM(tab);
      checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
      await toolbox.destroy();
    }

    info("Open toolbox inside RDM");
    {
      const { ui } = await openRDM(tab);
      const { toolbox, inspector } = await openInspector();
      inspector.walker.once("new-root", () => {
        ok(false, "Inspector saw new root, would reboot!");
      });
      checkToolbox(tab, ui.getViewportBrowser(), "inside RDM");
      await closeRDM(tab);
      checkToolbox(tab, tab.linkedBrowser, "after closing RDM");
      await toolbox.destroy();
    }

    await removeTab(tab);
  },
  { onlyPrefAndTask: true }
);
