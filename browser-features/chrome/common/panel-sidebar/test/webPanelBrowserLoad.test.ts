// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  getWebPanelContentBrowser,
  loadUriInWebPanelBrowser,
  type WebPanelBrowserElement,
} from "../utils/web-panel-browser.ts";
import { prepareWebPanelTab } from "../website-panel-window-child.ts";

function testWebPanelLoadUsesSupportedOptions(): void {
  const captured: { uri: nsIURI | null; options: object | null } = {
    uri: null,
    options: null,
  };
  const browser = {
    loadURI(uri: nsIURI, options?: object): void {
      captured.uri = uri;
      captured.options = options ?? null;
    },
  } as unknown as WebPanelBrowserElement;

  const targetURL = "https://example.com/panel-test";
  loadUriInWebPanelBrowser(browser, targetURL);

  assertEquals(
    captured.uri?.spec,
    targetURL,
    "should load the requested URI",
  );
  assert(captured.options !== null, "should pass load options");

  const options = captured.options as Record<string, unknown>;
  assert(
    options.triggeringPrincipal !== undefined,
    "should pass a triggering principal",
  );
  assert(
    !("remoteType" in options),
    "should not pass the removed remoteType option",
  );
  assert(
    !("remoteTypeOverride" in options),
    "should not force a remote type override for a web URL",
  );
}

function testUsesTabbrowserSelectedBrowser(): void {
  const selectedBrowser = {} as WebPanelBrowserElement;
  const chromeWindow = {
    gBrowser: { selectedBrowser },
    document: { getElementById: () => null },
  };
  const sidebarBrowser = {
    browsingContext: { associatedWindow: chromeWindow },
  };
  const parentWindow = {
    document: {
      getElementById: (id: string) =>
        id === "sidebar-panel-panel-id" ? sidebarBrowser : null,
    },
  } as unknown as Window;

  assertEquals(
    getWebPanelContentBrowser("panel-id", parentWindow),
    selectedBrowser,
    "should resolve the real tabbrowser content browser",
  );
}

function testPrepareWebPanelTabKeepsDefaultContext(): void {
  const tab = document.createXULElement("tab") as XULElement;
  const browser = {} as WebPanelBrowserElement;
  let addCount = 0;
  let removeCount = 0;
  const tabBrowser = {
    selectedTab: tab,
    selectedBrowser: browser,
    addTab: () => {
      addCount++;
      return document.createXULElement("tab") as XULElement;
    },
    removeTab: () => {
      removeCount++;
    },
  };

  assertEquals(
    prepareWebPanelTab(tabBrowser, 0),
    browser,
    "should return the tabbrowser-selected browser",
  );
  assertEquals(addCount, 0, "should reuse the default-context tab");
  assertEquals(removeCount, 0, "should not remove the reused tab");
  assertEquals(
    tab.getAttribute("BMS-webpanel-tab"),
    "true",
    "should mark the real tab as a web panel tab",
  );
}

function testPrepareWebPanelTabReplacesContainerContext(): void {
  const initialTab = document.createXULElement("tab") as XULElement;
  const replacementTab = document.createXULElement("tab") as XULElement;
  replacementTab.setAttribute("usercontextid", "3");
  const initialBrowser = {} as WebPanelBrowserElement;
  const replacementBrowser = {} as WebPanelBrowserElement;
  let selectedTab = initialTab;
  let removedTab: XULElement | null = null;
  let addedUserContextId = -1;
  const tabBrowser = {
    get selectedTab() {
      return selectedTab;
    },
    set selectedTab(tab: XULElement) {
      selectedTab = tab;
    },
    get selectedBrowser() {
      return selectedTab === replacementTab
        ? replacementBrowser
        : initialBrowser;
    },
    addTab: (
      url: string,
      options: { userContextId: number },
    ): XULElement => {
      assertEquals(url, "about:blank", "should create a blank replacement tab");
      addedUserContextId = options.userContextId;
      return replacementTab;
    },
    removeTab: (tab: XULElement) => {
      removedTab = tab;
    },
  };

  assertEquals(
    prepareWebPanelTab(tabBrowser, 3),
    replacementBrowser,
    "should return the replacement tab's browser",
  );
  assertEquals(addedUserContextId, 3, "should preserve the panel container");
  assertEquals(removedTab, initialTab, "should remove the initial tab");
  assertEquals(
    replacementTab.getAttribute("BMS-webpanel-tab"),
    "true",
    "should mark the replacement tab as a web panel tab",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("webPanelBrowserLoad.test.ts", [
    {
      name: "web panel loads with Gecko-supported options",
      fn: testWebPanelLoadUsesSupportedOptions,
    },
    {
      name: "web panel resolves the tabbrowser-selected browser",
      fn: testUsesTabbrowserSelectedBrowser,
    },
    {
      name: "web panel reuses the default-context tab",
      fn: testPrepareWebPanelTabKeepsDefaultContext,
    },
    {
      name: "web panel replaces the tab for a container context",
      fn: testPrepareWebPanelTabReplacesContainerContext,
    },
  ]);
}
