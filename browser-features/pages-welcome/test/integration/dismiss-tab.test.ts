// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser
import {
  assertEquals,
  runTests,
} from "../../../chrome/test/utils/test_harness.ts";
import type { DismissTestChromeHost, DismissTestUri } from "./types.ts";

export async function runAllTests() {
  const host = globalThis as unknown as DismissTestChromeHost;
  const { NRWelcomePageParent } = host.ChromeUtils.importESModule(
    "resource://noraneko/actors/NRWelcomePageParent.sys.mjs",
  );
  // Exercise the built parent actor with an isolated host, without closing
  // the test runner's window or changing the user's restored tabs.
  async function dismiss(other: "none" | "open" | "closing" | "hidden") {
    const tab = { isOpen: true, hidden: false, closing: false };
    const otherTab = {
      isOpen: other !== "closing",
      closing: other === "closing",
      hidden: other === "hidden",
    };
    let removed = false;
    let closeWindow = true;
    let destination = "";
    const browser = {
      ownerDocument: {
        defaultView: {
          gBrowser: {
            tabs: other === "none" ? [tab] : [tab, otherTab],
            getTabForBrowser: () => tab,
            removeTab: (
              _tab: unknown,
              options: { closeWindowWithLastTab: boolean },
            ) => {
              removed = true;
              closeWindow = options.closeWindowWithLastTab;
            },
          },
        },
      },
      loadURI: (uri: DismissTestUri) => {
        destination = uri.spec;
      },
    };
    const manager = {
      documentURI: host.Services.io.newURI("about:welcome?releaseNotes=1"),
    };
    await NRWelcomePageParent.prototype.receiveMessage.call({
      manager,
      browsingContext: {
        parent: null,
        currentWindowGlobal: manager,
        embedderElement: browser,
      },
    }, { name: "WelcomePage:dismiss" });
    return { removed, closeWindow, destination };
  }
  await runTests("dismiss-tab", [
    {
      name: "Last tab navigates in place instead of closing",
      fn: async () => {
        const result = await dismiss("none");
        assertEquals(result.removed, false, "Last tab removed");
        assertEquals(
          result.destination,
          "about:newtab",
          "New tab page not loaded",
        );
      },
    },
    {
      name: "Another open tab permits closure but never window closure",
      fn: async () => {
        const result = await dismiss("open");
        assertEquals(result.removed, true, "Prompt tab not removed");
        assertEquals(result.closeWindow, false, "Window closure not prevented");
        assertEquals(result.destination, "", "Unneeded new tab navigation");
      },
    },
    ...(["closing", "hidden"] as const).map((state) => ({
      name: `A ${state} sibling is not a usable remaining tab`,
      fn: async () => {
        const result = await dismiss(state);
        assertEquals(result.removed, false, "Last usable tab removed");
        assertEquals(
          result.destination,
          "about:newtab",
          "New tab page not loaded",
        );
      },
    })),
  ]);
}
