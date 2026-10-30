// SPDX-License-Identifier: MPL-2.0
// @colocated-env browser

import {
  assert,
  assertEquals,
  assertStringArrayEqual,
  runTests,
} from "../../../test/utils/test_harness.ts";
import {
  getWebPanelIdForBrowser,
  guardSharedPanelElements,
  routePopupNotifications,
} from "../utils/web-panel-popup-notifications.ts";
import type {
  ClearablePopupNotifications,
  PanelEventHandlerName,
  PopupNotification,
  PopupNotificationsLike,
  WebPanelPopupRouter,
} from "../utils/web-panel-types.ts";

const PANEL_URL =
  "chrome://browser/content/browser.xhtml?floorpWebPanelId=panel-1";

function fakeWindow(href: string, topChromeWindow: object | null): Window {
  return {
    location: { href },
    browsingContext: { topChromeWindow },
  } as unknown as Window;
}

function fakeBrowser(ownerWindow: Window): XULElement {
  return {
    ownerDocument: { defaultView: ownerWindow },
  } as unknown as XULElement;
}

function fakeNotification(browser: XULElement): PopupNotification {
  return { browser, owner: {} as PopupNotificationsLike };
}

interface Calls {
  originalShow: XULElement[];
  originalShowThis: unknown[];
  originalRemove: PopupNotification[][];
  originalLocationChange: XULElement[];
  routerShow: XULElement[];
  routerRemove: PopupNotification[][];
  routerLocationChange: XULElement[];
  originalPanelEvents: string[];
  routerPanelEvents: string[];
}

function createFixture(panelBrowsers: XULElement[]) {
  const calls: Calls = {
    originalShow: [],
    originalShowThis: [],
    originalRemove: [],
    originalLocationChange: [],
    routerShow: [],
    routerRemove: [],
    routerLocationChange: [],
    originalPanelEvents: [],
    routerPanelEvents: [],
  };
  const routedNotifications = new Set<PopupNotification>();
  const doc = document.implementation.createHTMLDocument("");
  const routerPanel = doc.createElement("div");
  const routerButton = doc.createElement("button");
  routerPanel.append(routerButton);
  const tabButton = doc.createElement("button");
  doc.body.append(routerPanel, tabButton);

  const target: PopupNotificationsLike = {
    show(browser) {
      calls.originalShow.push(browser);
      calls.originalShowThis.push(this);
      return fakeNotification(browser);
    },
    remove(notification) {
      calls.originalRemove.push(
        Array.isArray(notification) ? notification : [notification],
      );
    },
    locationChange(browser) {
      calls.originalLocationChange.push(browser);
    },
    _onButtonEvent(...args) {
      calls.originalPanelEvents.push(`_onButtonEvent:${args[1]}`);
    },
    _onMenuCommand() {
      calls.originalPanelEvents.push("_onMenuCommand");
    },
    _dismiss() {
      calls.originalPanelEvents.push("_dismiss");
    },
  };

  const router: WebPanelPopupRouter = {
    ownsBrowser: (browser) => panelBrowsers.includes(browser as XULElement),
    ownsNotification: (notification) => routedNotifications.has(notification),
    show(browser) {
      calls.routerShow.push(browser);
      const notification = fakeNotification(browser);
      routedNotifications.add(notification);
      return notification;
    },
    remove(notification) {
      calls.routerRemove.push(
        Array.isArray(notification) ? notification : [notification],
      );
    },
    locationChange(browser) {
      calls.routerLocationChange.push(browser);
    },
    ownsPanelElement: (element) => !!element && routerPanel.contains(element),
    handlePanelEvent(name: PanelEventHandlerName, args: unknown[]) {
      calls.routerPanelEvents.push(`${name}:${args[1] ?? ""}`);
    },
  };

  return { calls, target, router, routerButton, tabButton };
}

function testDetectsWebPanelBrowser(): void {
  const topWindow = fakeWindow("chrome://browser/content/browser.xhtml", null);
  const panelWindow = fakeWindow(PANEL_URL, topWindow);

  assertEquals(
    getWebPanelIdForBrowser(fakeBrowser(panelWindow), topWindow),
    "panel-1",
    "should return the panel id of a web panel content browser",
  );
}

function testIgnoresNonWebPanelBrowsers(): void {
  const topWindow = fakeWindow("chrome://browser/content/browser.xhtml", null);
  const otherTopWindow = fakeWindow(
    "chrome://browser/content/browser.xhtml",
    null,
  );

  assertEquals(
    getWebPanelIdForBrowser(fakeBrowser(topWindow), topWindow),
    null,
    "a normal tab browser is not a web panel browser",
  );
  assertEquals(
    getWebPanelIdForBrowser(
      fakeBrowser(
        fakeWindow("chrome://browser/content/browser.xhtml", topWindow),
      ),
      topWindow,
    ),
    null,
    "a nested window without the panel parameter is ignored",
  );
  assertEquals(
    getWebPanelIdForBrowser(
      fakeBrowser(fakeWindow(PANEL_URL, otherTopWindow)),
      topWindow,
    ),
    null,
    "a web panel of another browser window is ignored",
  );
  assertEquals(
    getWebPanelIdForBrowser(null, topWindow),
    null,
    "a missing browser is ignored",
  );
}

function testRoutesShowByBrowser(): void {
  const panelBrowser = {} as XULElement;
  const tabBrowser = {} as XULElement;
  const { calls, target, router } = createFixture([panelBrowser]);
  routePopupNotifications(target, () => router);

  target.show(panelBrowser, "webRTC-shareDevices");
  target.show(tabBrowser, "geolocation");

  assertEquals(calls.routerShow.length, 1, "panel prompt goes to the router");
  assertEquals(calls.routerShow[0], panelBrowser, "router gets the browser");
  assertEquals(calls.originalShow.length, 1, "tab prompt stays in place");
  assertEquals(calls.originalShow[0], tabBrowser, "original gets the tab");
  assertEquals(
    calls.originalShowThis[0],
    target,
    "original show keeps its receiver",
  );
}

function testFallsBackWithoutRouter(): void {
  const panelBrowser = {} as XULElement;
  const { calls, target } = createFixture([panelBrowser]);
  routePopupNotifications(target, () => null);

  target.show(panelBrowser, "geolocation");
  target.locationChange(panelBrowser);

  assertEquals(calls.originalShow.length, 1, "show falls back to original");
  assertEquals(
    calls.originalLocationChange.length,
    1,
    "locationChange falls back to original",
  );
}

function testSplitsRemoveByOwner(): void {
  const panelBrowser = {} as XULElement;
  const tabBrowser = {} as XULElement;
  const { calls, target, router } = createFixture([panelBrowser]);
  routePopupNotifications(target, () => router);

  const routed = target.show(panelBrowser, "webRTC-shareDevices");
  const local = target.show(tabBrowser, "geolocation");

  target.remove([routed, local], true);
  target.remove(local);

  assertEquals(calls.routerRemove.length, 1, "router removes its own");
  assertEquals(
    calls.routerRemove[0][0],
    routed,
    "router gets its notification",
  );
  assertEquals(calls.originalRemove.length, 2, "original removes the rest");
  assertEquals(calls.originalRemove[0][0], local, "array remove is split");
  assertEquals(
    calls.originalRemove[1][0],
    local,
    "single remove passes through",
  );
}

function testRoutesLocationChange(): void {
  const panelBrowser = {} as XULElement;
  const tabBrowser = {} as XULElement;
  const { calls, target, router } = createFixture([panelBrowser]);
  routePopupNotifications(target, () => router);

  target.locationChange(panelBrowser);
  target.locationChange(tabBrowser);

  assertEquals(calls.routerLocationChange[0], panelBrowser, "panel routed");
  assertEquals(calls.originalLocationChange[0], tabBrowser, "tab kept");
}

function testRoutingIsIdempotent(): void {
  const panelBrowser = {} as XULElement;
  const { calls, target, router } = createFixture([panelBrowser]);
  routePopupNotifications(target, () => router);
  const wrappedShow = target.show;
  routePopupNotifications(target, () => router);

  assert(target.show === wrappedShow, "second install must not re-wrap");
  target.show(panelBrowser, "geolocation");
  assertEquals(calls.routerShow.length, 1, "prompt is routed exactly once");
}

function testRoutesPanelEventsFromWebPanelPanel(): void {
  const { calls, target, router, routerButton, tabButton } = createFixture([]);
  routePopupNotifications(target, () => router);

  // popupnotification.js passes its own element as the 4th argument.
  target._onButtonEvent?.({}, "buttoncommand", "button", routerButton);
  target._onMenuCommand?.({ originalTarget: routerButton });
  target._dismiss?.({ target: routerButton }, true);
  assertStringArrayEqual(
    calls.routerPanelEvents,
    ["_onButtonEvent:buttoncommand", "_onMenuCommand:", "_dismiss:true"],
    "events from the web panel panel go to the web panel instance",
  );
  assertEquals(calls.originalPanelEvents.length, 0, "tab instance untouched");

  target._onButtonEvent?.({}, "buttoncommand", "button", tabButton);
  target._dismiss?.();
  assertStringArrayEqual(
    calls.originalPanelEvents,
    ["_onButtonEvent:buttoncommand", "_dismiss"],
    "tab events and internal dismissals stay with the tab instance",
  );
}

interface FakeClearable extends ClearablePopupNotifications {
  rounds: number;
}

/**
 * Mimics PopupNotifications._clearPanel() for an element whose recorded
 * original parent is the panel being cleared.
 */
function createSelfParentedPanel(withPopupSet: boolean): {
  instance: FakeClearable;
  popupSet: Element | null;
  child: Element;
} {
  const doc = document.implementation.createHTMLDocument("");
  const popupSet = withPopupSet ? doc.createElement("div") : null;
  if (popupSet) {
    popupSet.id = "mainPopupSet";
    doc.body.append(popupSet);
  }
  const panel = doc.createElement("div");
  const child = doc.createElement("span");
  panel.append(child);
  doc.body.append(panel);

  const instance: FakeClearable = {
    panel,
    rounds: 0,
    _clearPanel(this: FakeClearable) {
      let element;
      while ((element = this.panel.lastElementChild)) {
        if (++this.rounds > 10) {
          throw new Error("_clearPanel did not terminate");
        }
        this.panel.removeChild(element);
        this.panel.appendChild(element);
      }
    },
  };
  return { instance, popupSet, child };
}

function testGuardClearPanelBreaksSelfParentLoop(): void {
  const { instance, popupSet, child } = createSelfParentedPanel(true);
  guardSharedPanelElements(instance);
  instance._clearPanel();
  assertEquals(instance.rounds, 1, "the panel is emptied in one pass");
  assertEquals(child.parentNode, popupSet, "element goes to #mainPopupSet");

  instance.panel.appendChild(child);
  assertEquals(child.parentNode, instance.panel, "appendChild is restored");
}

function testGuardClearPanelWithoutPopupSet(): void {
  const { instance, child } = createSelfParentedPanel(false);
  guardSharedPanelElements(instance);
  guardSharedPanelElements(instance);
  instance._clearPanel();
  assertEquals(instance.rounds, 1, "the panel is emptied in one pass");
  assertEquals(child.parentNode, null, "element is detached");
}

function testRefreshPanelReleasesElementFromOtherPanel(): void {
  const doc = document.implementation.createHTMLDocument("");
  const popupSet = doc.createElement("div");
  popupSet.id = "mainPopupSet";
  const ownPanel = doc.createElement("panel");
  const otherPanel = doc.createElement("panel");
  const shared = doc.createElement("div");
  shared.id = "geolocation-notification";
  otherPanel.append(shared);
  const unrelated = doc.createElement("div");
  unrelated.id = "other-notification";
  otherPanel.append(unrelated);
  doc.body.append(popupSet, ownPanel, otherPanel);

  let parentSeenByRefresh: Node | null = null;
  const instance: ClearablePopupNotifications = {
    panel: ownPanel,
    _clearPanel() {},
    _refreshPanel() {
      parentSeenByRefresh = shared.parentNode;
    },
  };
  guardSharedPanelElements(instance);
  instance._refreshPanel?.([{ id: "geolocation" }]);

  assertEquals(
    parentSeenByRefresh,
    popupSet,
    "the element is back in #mainPopupSet before _refreshPanel runs",
  );
  assert(shared.hidden, "the released element is hidden");
  assertEquals(unrelated.parentNode, otherPanel, "other elements stay");

  ownPanel.append(shared);
  instance._refreshPanel?.([{ id: "geolocation" }]);
  assertEquals(
    parentSeenByRefresh,
    ownPanel,
    "an element in the panel being refreshed is left to _clearPanel",
  );
}

export async function runAllTests(): Promise<void> {
  await runTests("webPanelPopupNotifications.test.ts", [
    {
      name: "detects web panel content browsers",
      fn: testDetectsWebPanelBrowser,
    },
    {
      name: "ignores tab and foreign-window browsers",
      fn: testIgnoresNonWebPanelBrowsers,
    },
    { name: "routes show() by browser", fn: testRoutesShowByBrowser },
    {
      name: "falls back when the router is missing",
      fn: testFallsBackWithoutRouter,
    },
    { name: "splits remove() by owner", fn: testSplitsRemoveByOwner },
    { name: "routes locationChange()", fn: testRoutesLocationChange },
    { name: "routing is idempotent", fn: testRoutingIsIdempotent },
    {
      name: "routes popupnotification.js events by panel",
      fn: testRoutesPanelEventsFromWebPanelPanel,
    },
    {
      name: "guards _clearPanel against self-parented elements",
      fn: testGuardClearPanelBreaksSelfParentLoop,
    },
    {
      name: "releases elements shown by another panel before refreshing",
      fn: testRefreshPanelReleasesElementFromOtherPanel,
    },
    {
      name: "guards _clearPanel without #mainPopupSet",
      fn: testGuardClearPanelWithoutPopupSet,
    },
  ]);
}
