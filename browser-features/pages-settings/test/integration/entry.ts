// SPDX-License-Identifier: MPL-2.0
import {
  calls,
  expose,
  installBridge,
  prefs,
} from "../../../../libs/ui/test/page-fixture.ts";
import { runPageTests } from "./settings.test.ts";
import { runAdvancedTests } from "./advanced.test.ts";
import { setupManagement } from "./management-bridge.ts";
import { runManagementTests } from "./management.test.ts";

installBridge();
prefs.set("floorp.os.hidden", false);
prefs.set(
  "floorp.design.configs",
  JSON.stringify({
    globalConfigs: { userInterface: "lepton", faviconColor: false },
    tabbar: {
      tabbarPosition: "default",
      tabbarStyle: "horizontal",
      multiRowTabBar: { maxRowEnabled: false, maxRow: 3 },
    },
    tab: {
      tabScroll: { enabled: true, reverse: false, wrap: false },
      tabOpenPosition: -1,
      tabMinHeight: 30,
      tabMinWidth: 100,
      tabPinTitle: false,
      tabDubleClickToClose: false,
    },
    uiCustomization: {
      navbar: { position: "top", searchBarTop: false, futureKey: "keep" },
      display: {
        disableFullscreenNotification: false,
        deleteBrowserBorder: false,
      },
      special: {
        optimizeForTreeStyleTab: false,
        hideForwardBackwardButton: false,
        stgLikeWorkspaces: false,
      },
      multirowTab: { newtabInsideEnabled: false },
      bookmarkBar: { focusExpand: false, position: "top" },
      qrCode: { disableButton: false },
      disableFloorpStart: false,
    },
    futureKey: "keep",
  }),
);
for (
  const [key, value] of Object.entries({
    "floorp.panelSidebar.enabled": true,
    "floorp.panelSidebar.config": JSON.stringify({
      autoUnload: true,
      position_start: false,
      globalWidth: 400,
      displayed: true,
      webExtensionRunningEnabled: false,
      futureKey: "keep",
    }),
    "floorp.panelSidebar.data": '{"data":[]}',
    "floorp.workspaces.enabled": true,
    "floorp.workspaces.v4.config": JSON.stringify({
      manageOnBms: false,
      showWorkspaceNameOnToolbar: false,
      closePopupAfterClick: true,
      exitOnLastTabClose: false,
      futureKey: "keep",
    }),
    "floorp.browser.ssb.enabled": true,
    "floorp.browser.ssb.config": '{"showToolbar":false,"futureKey":"keep"}',
    "floorp.mousegesture.enabled": true,
    "floorp.keyboardshortcut.enabled": true,
    "floorp.memory.idleReclaim": JSON.stringify({
      enabled: true,
      idleThresholdSec: 60,
      minIntervalSec: 300,
      minResidentMB: 400,
      pollIntervalSec: 60,
    }),
    "browser.link.open_newwindow": 3,
    "browser.taskbar.previews.enable": true,
  })
) prefs.set(key, value);
for (
  const name of [
    "NRGetInstalledApps",
    "NRGetContainers",
    "NRGetContainerContexts",
    "NRGetStaticPanels",
    "NRGetExtensionPanels",
  ]
) {
  expose(
    name,
    (callback: (value: string) => void) =>
      callback(name === "NRGetInstalledApps" ? "{}" : "[]"),
  );
}
expose("NRInitializeWorkspaces", (...args: unknown[]) => {
  calls.push({ method: "NRInitializeWorkspaces", args });
  return Promise.resolve({ success: true });
});
expose("ChromeUtils", {
  importESModule: () => ({
    AppConstants: { platform: "win" },
    Experiments: {
      getActiveExperiments: () => [],
      getAllExperiments: () => [],
      init: () => calls.push({ method: "experiments.init", args: [] }),
      clearCache: () =>
        calls.push({ method: "experiments.clearCache", args: [] }),
    },
  }),
});
setupManagement();
await import("../../src/main.tsx");
if (new URLSearchParams(location.search).get("suite") === "advanced") {
  await runAdvancedTests();
} else if (new URLSearchParams(location.search).get("suite") === "management") {
  await runManagementTests();
} else await runPageTests();
