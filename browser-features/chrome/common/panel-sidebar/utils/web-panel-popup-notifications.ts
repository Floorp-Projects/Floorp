/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Permission prompts (microphone, camera, geolocation, notifications, ...)
 * for web panels.
 *
 * A web panel's content browser lives inside a nested browser.xhtml, so Gecko
 * never shows its PopupNotifications:
 * - WebRTCParent shows the prompt on the top chrome window, whose
 *   `_isActiveBrowser()` only accepts the selected tab.
 * - PermissionUI shows it on the nested window, which is never
 *   `Services.focus.activeWindow`.
 *
 * Firefox solves the same problem for its own sidebar with
 * `SidebarPopupNotifications` (browser/components/sidebar/sidebar-permissions.mjs).
 * We follow that model: the top chrome window owns a dedicated
 * PopupNotifications instance for web panel browsers, and both windows
 * forward web panel requests to it. Like tabs, only the web panel that is
 * shown is "active": prompts of panels in the background wait until their
 * panel is shown again.
 */

import {
  isWebPanelChildUrl,
  WEB_PANEL_URL_PARAM,
} from "./web-panel-context.ts";
import type {
  BrowserElement,
  ClearablePopupNotifications,
  PanelEventHandlerName,
  PopupNotification,
  PopupNotificationsConstructor,
  PopupNotificationsInstance,
  PopupNotificationsLike,
  TabbrowserShim,
  WebPanelHostElement,
  WebPanelPopupChromeWindow,
  WebPanelPopupRouter,
  WebPanelPopupRouterHost,
} from "./web-panel-types.ts";

const WEB_PANEL_NOTIFICATION_POPUP_ID = "floorp-webpanel-notification-popup";
const WEB_PANEL_NOTIFICATION_ICON_BOX_ID =
  "floorp-webpanel-notification-popup-box";

/**
 * popupnotification.js calls these on its window's global PopupNotifications
 * (the tab instance), even for notifications shown by another instance in the
 * same document. `_dismiss` is only routed when called with an event.
 */
const PANEL_EVENT_HANDLERS: readonly PanelEventHandlerName[] = [
  "_onButtonEvent",
  "_onMenuCommand",
  "_onCheckboxCommand",
  "_dismiss",
];

const VISIBILITY_OPTIONS = { visibilityProperty: true };

function isVisible(element: Element | null | undefined): element is Element {
  return !!element?.checkVisibility?.(VISIBILITY_OPTIONS);
}

/** Panels that are not selected stay in the DOM with no size. */
function isShown(element: Element): boolean {
  if (!isVisible(element)) {
    return false;
  }
  const rect = element.getBoundingClientRect();
  return rect.width > 0 && rect.height > 0;
}

/**
 * window.PopupNotifications is a lazy getter that needs gBrowser; touching it
 * before the top window's delayed startup could cache a broken value.
 */
function afterDelayedStartup(
  chromeWindow: WebPanelPopupChromeWindow,
  callback: () => void,
): void {
  const delayedStartup = chromeWindow.delayedStartupPromise;
  if (!delayedStartup) {
    callback();
    return;
  }
  delayedStartup.then(callback).catch((error: unknown) => {
    console.error("[WebPanelPopupNotifications]", error);
  });
}

/**
 * Returns the web panel id when `browser` is a web panel content browser
 * embedded in `chromeWindow`, otherwise null.
 */
export function getWebPanelIdForBrowser(
  browser: Element | null | undefined,
  chromeWindow: Window,
): string | null {
  const ownerWindow = browser?.ownerDocument?.defaultView as
    | WebPanelPopupChromeWindow
    | null
    | undefined;
  if (!ownerWindow || ownerWindow === chromeWindow) {
    return null;
  }

  let href: string;
  try {
    href = ownerWindow.location.href;
  } catch {
    return null;
  }
  if (!isWebPanelChildUrl(href)) {
    return null;
  }
  if (ownerWindow.browsingContext?.topChromeWindow !== chromeWindow) {
    return null;
  }

  return new URL(href).searchParams.get(WEB_PANEL_URL_PARAM);
}

/**
 * Returns the element a popupnotification.js handler call is about: the
 * notification element passed to _onButtonEvent, or the event target.
 */
function getPanelEventElement(args: unknown[]): Element | null {
  const candidates = [
    args[3],
    (args[0] as { originalTarget?: unknown } | null | undefined)
      ?.originalTarget,
    (args[0] as { target?: unknown } | null | undefined)?.target,
  ];
  for (const candidate of candidates) {
    if (typeof (candidate as Element | null)?.closest === "function") {
      return candidate as Element;
    }
  }
  return null;
}

/**
 * The tab and web panel PopupNotifications instances share one chrome
 * document, and therefore its `<popupnotification id="...">` elements
 * (templates such as webRTC-shareDevices-notification, and ad-hoc ones such
 * as geolocation-notification). _refreshPanel() takes the element by id even
 * when the other instance is displaying it, which empties the other panel and
 * records that panel as the element's original parent; that panel's
 * _clearPanel() then re-appends the element to the panel it is emptying,
 * forever.
 *
 * Before _refreshPanel() takes an element from another panel, return it to
 * #mainPopupSet, where the chrome document keeps its templates, so that is
 * what gets recorded as its original parent. The other panel is left alone:
 * closing it re-shows its persistent prompts, which take the element back.
 * _clearPanel() also never puts an element back into the panel it empties.
 */
export function guardSharedPanelElements(
  instance: ClearablePopupNotifications,
): void {
  if (instance.floorpClearPanelGuarded) {
    return;
  }
  instance.floorpClearPanelGuarded = true;

  const originalRefreshPanel = instance._refreshPanel;
  if (originalRefreshPanel) {
    instance._refreshPanel = function (
      this: ClearablePopupNotifications,
      notifications: { id: string }[],
    ) {
      releaseElementsFromOtherPanels(this.panel, notifications);
      return originalRefreshPanel.call(this, notifications);
    };
  }

  const originalClearPanel = instance._clearPanel;
  instance._clearPanel = function (this: ClearablePopupNotifications) {
    const panel = this.panel;
    const appendChild = panel.appendChild;
    panel.appendChild = function <T extends Node>(this: Element, node: T): T {
      const home = panel.ownerDocument.getElementById("mainPopupSet");
      if (home && home !== panel) {
        home.appendChild(node);
        return node;
      }
      (node as unknown as ChildNode).remove?.();
      return node;
    };
    try {
      return originalClearPanel.call(this);
    } finally {
      // Drop the own property so the prototype's appendChild applies again.
      delete (panel as { appendChild?: unknown }).appendChild;
      if (panel.appendChild !== appendChild) {
        panel.appendChild = appendChild;
      }
    }
  };
}

function releaseElementsFromOtherPanels(
  panel: Element,
  notifications: { id: string }[] | null | undefined,
): void {
  const doc = panel.ownerDocument;
  const home = doc.getElementById("mainPopupSet");
  if (!home) {
    return;
  }
  for (const notification of notifications ?? []) {
    const element = doc.getElementById(`${notification.id}-notification`);
    const otherPanel = element?.parentElement;
    if (
      !element || !otherPanel || otherPanel === panel || otherPanel === home ||
      otherPanel.localName !== "panel"
    ) {
      continue;
    }
    element.hidden = true;
    home.append(element);
  }
}

/**
 * Wraps a window's PopupNotifications so that requests for web panel browsers
 * go to the web panel instance. The router is resolved per call, so the
 * nested window never holds on to it.
 */
export function routePopupNotifications(
  target: PopupNotificationsLike,
  resolveRouter: () => WebPanelPopupRouter | null,
): void {
  if (target.floorpWebPanelRouted) {
    return;
  }
  target.floorpWebPanelRouted = true;

  const originalShow = target.show;
  const originalRemove = target.remove;
  const originalLocationChange = target.locationChange;

  target.show = function (browser, ...args) {
    const router = resolveRouter();
    if (router?.ownsBrowser(browser)) {
      return router.show(browser, ...args);
    }
    return originalShow.call(this, browser, ...args);
  };

  target.remove = function (notification, withoutUserResponse) {
    const router = resolveRouter();
    const notifications = Array.isArray(notification)
      ? notification
      : [notification];
    const routed = router
      ? notifications.filter((n) => router.ownsNotification(n))
      : [];
    const rest = notifications.filter((n) => !routed.includes(n));

    if (router && routed.length) {
      router.remove(routed, withoutUserResponse);
    }
    if (rest.length) {
      originalRemove.call(
        this,
        Array.isArray(notification) ? rest : rest[0],
        withoutUserResponse,
      );
    }
  };

  target.locationChange = function (browser) {
    const router = resolveRouter();
    if (router?.ownsBrowser(browser)) {
      router.locationChange(browser);
      return;
    }
    originalLocationChange.call(this, browser);
  };

  for (const name of PANEL_EVENT_HANDLERS) {
    const original = target[name];
    if (typeof original !== "function") {
      continue;
    }
    target[name] = function (this: PopupNotificationsLike, ...args: unknown[]) {
      const router = resolveRouter();
      if (router?.ownsPanelElement(getPanelEventElement(args))) {
        return router.handlePanelEvent(name, args);
      }
      return original.apply(this, args);
    };
  }
}

export class WebPanelPopupNotifications implements WebPanelPopupRouterHost {
  private notifications: PopupNotificationsInstance | null = null;
  private panel: XULPopupElement | null = null;

  constructor(private readonly chromeWindow: WebPanelPopupChromeWindow) {}

  /** Installs the router on a top-level browser window (idempotent). */
  static install(
    chromeWindow: WebPanelPopupChromeWindow =
      globalThis as unknown as WebPanelPopupChromeWindow,
  ): WebPanelPopupRouterHost {
    const existing = chromeWindow.floorpWebPanelPopupNotifications;
    if (existing) {
      return existing;
    }

    const router = new WebPanelPopupNotifications(chromeWindow);
    chromeWindow.floorpWebPanelPopupNotifications = router;

    const routeMainNotifications = () => {
      const mainNotifications = chromeWindow.PopupNotifications;
      if (!mainNotifications) {
        console.error(
          "[WebPanelPopupNotifications] PopupNotifications is unavailable",
        );
        return;
      }
      routePopupNotifications(
        mainNotifications,
        () => chromeWindow.floorpWebPanelPopupNotifications ?? null,
      );
      guardSharedPanelElements(
        mainNotifications as unknown as ClearablePopupNotifications,
      );
    };

    afterDelayedStartup(chromeWindow, routeMainNotifications);
    return router;
  }

  /**
   * Called from a web panel's nested browser.xhtml: forwards its prompts to
   * the router of the top chrome window that embeds it.
   */
  static forwardFromWebPanelWindow(
    panelWindow: WebPanelPopupChromeWindow =
      globalThis as unknown as WebPanelPopupChromeWindow,
  ): void {
    // The caller has already waited for gBrowser, which is all the lazy
    // PopupNotifications getter needs. The nested window's delayed startup is
    // not awaited because it is not guaranteed to run for embedded windows.
    const panelNotifications = panelWindow.PopupNotifications;
    if (!panelNotifications) {
      console.error(
        "[WebPanelPopupNotifications] Web panel window has no PopupNotifications",
      );
      return;
    }

    routePopupNotifications(panelNotifications, () => {
      const topWindow = panelWindow.browsingContext?.topChromeWindow as
        | WebPanelPopupChromeWindow
        | null
        | undefined;
      return topWindow?.floorpWebPanelPopupNotifications ?? null;
    });
  }

  ownsBrowser(browser: Element | null | undefined): boolean {
    return getWebPanelIdForBrowser(browser, this.chromeWindow) !== null;
  }

  ownsNotification(notification: PopupNotification): boolean {
    return (
      this.notifications !== null && notification.owner === this.notifications
    );
  }

  show(browser: BrowserElement, ...args: unknown[]): PopupNotification {
    return this.getNotifications().show(browser, ...args);
  }

  remove(
    notification: PopupNotification | PopupNotification[],
    withoutUserResponse?: boolean,
  ): void {
    this.notifications?.remove(notification, withoutUserResponse);
  }

  locationChange(browser: BrowserElement): void {
    this.notifications?.locationChange(browser);
  }

  ownsPanelElement(element: Element | null): boolean {
    return !!element && !!this.panel?.contains(element);
  }

  handlePanelEvent(name: PanelEventHandlerName, args: unknown[]): unknown {
    return this.notifications?.[name]?.(...args);
  }

  /**
   * Call when the shown web panel may have changed: shows the prompts the
   * newly shown panel asked for in the background, like a tab switch does.
   */
  refresh(): void {
    const notifications = this.notifications;
    if (!notifications) {
      return;
    }
    // Wait for the panel sidebar to lay out the newly selected panel.
    this.chromeWindow.requestAnimationFrame(() => {
      const browser = this.getShownBrowser();
      if (browser) {
        // Prompts hidden by switching panels have no anchor icon to reopen
        // them from, so show them again with their panel.
        const pending = notifications.getNotificationsForBrowser(browser);
        for (const notification of pending) {
          notification.dismissed = false;
        }
      }
      notifications._update();
    });
  }

  /** The content browser of the web panel that is currently shown, if any. */
  private getShownBrowser(): BrowserElement | null {
    const hosts = this.chromeWindow.document.querySelectorAll(
      ".sidebar-panel-browser",
    );
    for (const host of hosts) {
      if (!isShown(host)) {
        continue;
      }
      const browser = (host as WebPanelHostElement).contentWindow
        ?.floorpWebPanelContentBrowser;
      if (browser && this.ownsBrowser(browser)) {
        return browser;
      }
    }
    return null;
  }

  private getNotifications(): PopupNotificationsInstance {
    if (this.notifications) {
      return this.notifications;
    }

    const doc = this.chromeWindow.document;
    const popupSet = doc.getElementById("mainPopupSet") ??
      doc.documentElement;

    // Own panel instead of sharing #notification-popup, so tab prompts and
    // web panel prompts never close or dismiss each other.
    const panel = doc.createXULElement("panel") as XULPopupElement;
    panel.id = WEB_PANEL_NOTIFICATION_POPUP_ID;
    panel.setAttribute("type", "arrow");
    panel.setAttribute("position", "after_start");
    panel.setAttribute("hidden", "true");
    panel.setAttribute("orient", "vertical");
    panel.setAttribute("noautofocus", "true");
    panel.setAttribute("role", "alert");
    panel.setAttribute("remote", "true");

    // Notifications resolve their anchor through the icon box; this hidden
    // box is only a stable anchor that getVisibleAnchor() replaces.
    const iconBox = doc.createXULElement("box") as XULElement;
    iconBox.id = WEB_PANEL_NOTIFICATION_ICON_BOX_ID;
    iconBox.setAttribute("hidden", "true");

    popupSet?.append(panel, iconBox);

    const { PopupNotifications } = ChromeUtils.importESModule(
      "resource://gre/modules/PopupNotifications.sys.mjs",
    ) as { PopupNotifications: PopupNotificationsConstructor };

    const getShownBrowser = () => this.getShownBrowser();
    const tabbrowser: TabbrowserShim = {
      documentGlobal: this.chromeWindow,
      get selectedBrowser() {
        return getShownBrowser();
      },
      // getVisibleAnchor() always finds an anchor while a panel is shown, so
      // never fall back to the selected tab, which belongs to another site.
      selectedTab: null,
    };

    const notifications = new PopupNotifications(tabbrowser, panel, iconBox, {
      getVisibleAnchorElement: () => this.getVisibleAnchor(),
    });
    // Like a tab, only the web panel that is shown is active. The top window
    // is the one that has focus.
    notifications._isActiveBrowser = (browser) =>
      browser === this.getShownBrowser();
    guardSharedPanelElements(
      notifications as unknown as ClearablePopupNotifications,
    );

    this.panel = panel;
    this.notifications = notifications;
    return notifications;
  }

  private getVisibleAnchor(): Element | null {
    const doc = this.chromeWindow.document;
    const panelId = getWebPanelIdForBrowser(
      this.getShownBrowser(),
      this.chromeWindow,
    );
    if (!panelId) {
      return null;
    }
    const candidates = [
      doc.getElementById("panel-sidebar-header"),
      doc.querySelector(
        `.panel-sidebar-panel[data-panel-id="${CSS.escape(panelId)}"]`,
      ),
      doc.getElementById(`sidebar-panel-${panelId}`),
    ];
    return candidates.find(isVisible) ?? null;
  }
}
