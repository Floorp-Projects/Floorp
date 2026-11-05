/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type BrowserElement = XULElement;

/** Handlers popupnotification.js calls on its window's PopupNotifications. */
export type PanelEventHandlerName =
  | "_onButtonEvent"
  | "_onMenuCommand"
  | "_onCheckboxCommand"
  | "_dismiss";

type PanelEventHandlers = {
  [K in PanelEventHandlerName]?: (...args: unknown[]) => unknown;
};

export interface PopupNotification {
  browser: BrowserElement;
  owner: PopupNotificationsLike;
  dismissed?: boolean;
}

export interface PopupNotificationsLike extends PanelEventHandlers {
  show(browser: BrowserElement, ...args: unknown[]): PopupNotification;
  remove(
    notification: PopupNotification | PopupNotification[],
    withoutUserResponse?: boolean,
  ): void;
  locationChange(browser: BrowserElement): void;
  floorpWebPanelRouted?: boolean;
}

/** What a wrapped PopupNotifications needs from the web panel router. */
export interface WebPanelPopupRouter {
  ownsBrowser(browser: Element | null | undefined): boolean;
  ownsNotification(notification: PopupNotification): boolean;
  show(browser: BrowserElement, ...args: unknown[]): PopupNotification;
  remove(
    notification: PopupNotification | PopupNotification[],
    withoutUserResponse?: boolean,
  ): void;
  locationChange(browser: BrowserElement): void;
  ownsPanelElement(element: Element | null): boolean;
  handlePanelEvent(name: PanelEventHandlerName, args: unknown[]): unknown;
}

export interface PopupNotificationsInstance extends PopupNotificationsLike {
  _isActiveBrowser(browser: BrowserElement): boolean;
  _update(): void;
  getNotificationsForBrowser(browser: BrowserElement): PopupNotification[];
}

/** The parts of a PopupNotifications instance that guardSharedPanelElements
 * touches. */
export interface ClearablePopupNotifications {
  panel: Element;
  _clearPanel(): void;
  _refreshPanel?(notifications: { id: string }[]): void;
  floorpClearPanelGuarded?: boolean;
}

/** Minimal stand-in for the tabbrowser that PopupNotifications expects. */
export interface TabbrowserShim {
  readonly documentGlobal: Window;
  readonly selectedBrowser: BrowserElement | null;
  readonly selectedTab: Element | null;
}

export type PopupNotificationsConstructor = new (
  tabbrowser: TabbrowserShim,
  panel: XULPopupElement,
  iconBox: XULElement | null,
  options?: {
    getVisibleAnchorElement?: (anchor: Element | null) => Element | null;
  },
) => PopupNotificationsInstance;

export type WebPanelPopupChromeWindow = Window & {
  PopupNotifications?: PopupNotificationsLike;
  floorpWebPanelPopupNotifications?: WebPanelPopupRouterHost;
  delayedStartupPromise?: Promise<unknown>;
  browsingContext?: { topChromeWindow?: Window | null } | null;
};

/** The router as stored on the top chrome window. */
export interface WebPanelPopupRouterHost extends WebPanelPopupRouter {
  refresh(): void;
}

/** A panel host `<browser>` whose nested window holds a web panel. */
export type WebPanelHostElement = Element & {
  contentWindow?: { floorpWebPanelContentBrowser?: BrowserElement } | null;
};

/** The parts of Services.ppmm.sharedData the web panel registry uses. */
export interface WritableSharedDataLike {
  get(key: string): unknown;
  set(key: string, value: unknown): void;
  delete(key: string): void;
  flush(): void;
}
