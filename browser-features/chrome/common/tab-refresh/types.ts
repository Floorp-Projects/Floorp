/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export type HoverReloadTab = Element & {
  closing?: boolean;
};

export type HoverReloadBrowser = {
  tabs: ArrayLike<HoverReloadTab>;
  tabContainer: Element;
  reloadTab: (tab: HoverReloadTab) => void;
};

export type HoverReloadPrefObserver = (
  subject: unknown,
  topic: string,
  data: string,
) => void;

export type HoverReloadPrefs = {
  getBoolPref: (name: string, fallback: boolean) => boolean;
  addObserver: (name: string, observer: HoverReloadPrefObserver) => void;
  removeObserver: (name: string, observer: HoverReloadPrefObserver) => void;
};

export type HoverReloadTimerHandle =
  | number
  | ReturnType<typeof globalThis.setTimeout>;

export type HoverReloadClock = {
  setTimeout: (
    callback: () => void,
    delay: number,
  ) => HoverReloadTimerHandle;
  clearTimeout: (handle: HoverReloadTimerHandle) => void;
};

export type HoverReloadMutationObserver = {
  observe: (target: Node, options?: MutationObserverInit) => void;
  disconnect: () => void;
};

export type HoverReloadMutationObserverFactory = (
  callback: MutationCallback,
) => HoverReloadMutationObserver;

export type HoverReloadUnloadTarget = Pick<
  EventTarget,
  "addEventListener" | "removeEventListener"
>;

export type HoverReloadDocument = Document & {
  createXULElement?: (localName: string) => XULElement;
};

export type HoverReloadControllerOptions = {
  browser: HoverReloadBrowser;
  document: HoverReloadDocument;
  prefs: HoverReloadPrefs;
  clock?: HoverReloadClock;
  mutationObserverFactory?: HoverReloadMutationObserverFactory;
  unloadTarget?: HoverReloadUnloadTarget;
  label?: string;
  hoverDelayMs?: number;
};
