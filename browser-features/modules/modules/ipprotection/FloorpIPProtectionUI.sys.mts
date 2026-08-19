// SPDX-License-Identifier: MPL-2.0

import type { FloorpIPProtectionDisclosureStrings } from "./FloorpIPProtectionDisclosure.sys.mts";

const BLOCK_CALLOUTS_PREF = "browser.ipProtection.blockIPProtectionCallouts";
const ENABLED_PREF = "browser.ipProtection.enabled";
const MAX_PANEL_RETRY_FRAMES = 120;
const READY_ATTRIBUTE = "data-floorp-ipprotection-ready";
const TOOLBAR_BUTTON_ID = "ipprotection-button";
const PANEL_ID = "PanelUI-ipprotection";
const PANEL_CONTENT_ID = "PanelUI-ipprotection-content";
const HEADER_CONTENT_ID = "ipprotection-header-content";
const HEADER_BUTTON_ID = "ipprotection-header-button";
const SCOPE_ID = "floorp-ipprotection-scope";
const FULL_DISCLOSURE_ID = "floorp-ipprotection-full-disclosure";
const OUTER_STYLE_ID = "floorp-ipprotection-outer-style";
const INNER_STYLE_ID = "floorp-ipprotection-inner-style";

const GUARD_STYLES = `
#${TOOLBAR_BUTTON_ID}:not([${READY_ATTRIBUTE}="true"]),
#${PANEL_ID}:not([${READY_ATTRIBUTE}="true"]) {
  visibility: hidden !important;
  pointer-events: none !important;
}

@-moz-document url-prefix("about:preferences"), url-prefix("about:settings") {
  setting-group[groupid="ipprotection"]:not([${READY_ATTRIBUTE}="true"]) {
    visibility: hidden !important;
    pointer-events: none !important;
  }
}
`;

const OUTER_SHADOW_STYLES = `
#${SCOPE_ID} {
  color: var(--text-color-deemphasized);
  border-block-start: 1px solid var(--panel-separator-color);
  margin-block-start: var(--space-medium);
  padding: var(--space-medium) var(--space-large) 0;
  line-height: 1.4;
}
`;

const INNER_SHADOW_STYLES = `
#${FULL_DISCLOSURE_ID} {
  color: var(--text-color-deemphasized);
  background-color: var(--background-color-box-info);
  border-radius: var(--border-radius-medium);
  margin-block-end: var(--space-medium);
  padding: var(--space-medium);
  line-height: 1.4;
}
`;

type CalloutMessage = Readonly<{ id?: unknown }>;
type FeatureCalloutMessagesLike = {
  getMessages(...args: unknown[]): unknown;
};

type BrowserWindow = Window & {
  MutationObserver: typeof MutationObserver;
};

type WindowObserver = {
  documentObserver: MutationObserver;
  toolbarObserver: MutationObserver | null;
  panelHeaderObserver: MutationObserver | null;
  panelOuterObserver: MutationObserver | null;
  panelInnerObserver: MutationObserver | null;
  toolbarNode: Element | null;
  panelHeaderNodes: readonly [Element, Element] | null;
  panelOuterRoot: ShadowRoot | null;
  panelInnerRoot: ShadowRoot | null;
  panelRetryFrame: number | null;
  panelRetryAttempts: number;
  panelRepairing: boolean;
};

const observedWindows = new WeakMap<BrowserWindow, WindowObserver>();
let guardSheetURI: nsIURI | null = null;
let calloutHookInstalled = false;
let windowObserverInstalled = false;
let earlyInstallResult: boolean | null = null;

export function filterFloorpIPProtectionCallouts<T extends CalloutMessage>(
  messages: readonly T[],
): T[] {
  return messages.filter(
    (message) =>
      typeof message.id !== "string" ||
      !message.id.startsWith("IP_PROTECTION_"),
  );
}

export function resolveFloorpIPProtectionToolbarTooltip(
  classNames: Iterable<string>,
  strings: FloorpIPProtectionDisclosureStrings,
): string {
  const classes = new Set(classNames);
  if (
    classes.has("ipprotection-error") ||
    classes.has("ipprotection-network-error")
  ) {
    return strings.toolbarErrorTooltip;
  }
  if (classes.has("ipprotection-paused")) {
    return strings.toolbarPausedTooltip;
  }
  if (classes.has("ipprotection-excluded")) {
    return strings.toolbarExcludedTooltip;
  }
  if (classes.has("ipprotection-on")) {
    return strings.toolbarActiveTooltip;
  }
  return strings.toolbarInactiveTooltip;
}

function getDisclosureStrings(): FloorpIPProtectionDisclosureStrings {
  const { getFloorpIPProtectionDisclosureStrings } = ChromeUtils.importESModule(
    "resource://noraneko/modules/ipprotection/FloorpIPProtectionDisclosure.sys.mjs",
  );
  return getFloorpIPProtectionDisclosureStrings();
}

function setAttributeIfChanged(
  element: Element,
  name: string,
  value: string,
): void {
  if (element.getAttribute(name) !== value) {
    element.setAttribute(name, value);
  }
}

function setTextIfChanged(element: Element, value: string): void {
  if (element.textContent !== value) {
    element.textContent = value;
  }
}

function markReady(element: Element): void {
  setAttributeIfChanged(element, READY_ATTRIBUTE, "true");
}

function markNotReady(element: Element | null): void {
  element?.removeAttribute(READY_ATTRIBUTE);
}

function ensureStyle(
  root: ShadowRoot,
  id: string,
  cssText: string,
): void {
  let style = root.getElementById(id) as HTMLStyleElement | null;
  if (!style) {
    const doc = root.ownerDocument;
    if (!doc) {
      return;
    }
    style = doc.createElement("style");
    style.id = id;
    root.prepend(style);
  }
  if (style.textContent !== cssText) {
    style.textContent = cssText;
  }
}

function registerGuardSheet(): void {
  if (guardSheetURI) {
    return;
  }
  const styleSheetService = Cc[
    "@mozilla.org/content/style-sheet-service;1"
  ].getService(Ci.nsIStyleSheetService);
  const uri = Services.io.newURI(
    `data:text/css;charset=utf-8,${encodeURIComponent(GUARD_STYLES)}`,
  );
  const sheetType = Ci.nsIStyleSheetService.AGENT_SHEET as number;
  if (!styleSheetService.sheetRegistered(uri, sheetType)) {
    styleSheetService.loadAndRegisterSheet(uri, sheetType);
  }
  guardSheetURI = uri;
}

function installCalloutHook(): void {
  if (calloutHookInstalled) {
    return;
  }
  const { FeatureCalloutMessages } = ChromeUtils.importESModule(
    "resource:///modules/asrouter/FeatureCalloutMessages.sys.mjs",
  ) as { FeatureCalloutMessages: FeatureCalloutMessagesLike };
  const originalGetMessages = FeatureCalloutMessages.getMessages;
  FeatureCalloutMessages.getMessages = function (
    this: FeatureCalloutMessagesLike,
    ...args: unknown[]
  ): unknown {
    const messages = Reflect.apply(originalGetMessages, this, args);
    if (!Array.isArray(messages)) {
      return messages;
    }
    if (!Services.prefs.getBoolPref(BLOCK_CALLOUTS_PREF, false)) {
      return messages;
    }
    return filterFloorpIPProtectionCallouts(messages as CalloutMessage[]);
  };
  calloutHookInstalled = true;
}

function updateToolbarButton(
  button: Element,
  strings: FloorpIPProtectionDisclosureStrings,
): void {
  button.removeAttribute("data-l10n-id");
  setAttributeIfChanged(button, "label", strings.title);
  const tooltip = resolveFloorpIPProtectionToolbarTooltip(
    Array.from(button.classList).filter(
      (className): className is string => className !== null,
    ),
    strings,
  );
  setAttributeIfChanged(button, "tooltiptext", tooltip);
  setAttributeIfChanged(button, "aria-label", tooltip);
  markReady(button);
}

function updatePanelHeader(
  doc: Document,
  strings: FloorpIPProtectionDisclosureStrings,
): boolean {
  const header = doc.getElementById(HEADER_CONTENT_ID);
  const helpButton = doc.getElementById(HEADER_BUTTON_ID);
  if (!header || !helpButton) {
    return false;
  }
  header.removeAttribute("data-l10n-id");
  setTextIfChanged(header, strings.title);
  helpButton.removeAttribute("data-l10n-id");
  setAttributeIfChanged(helpButton, "tooltiptext", strings.helpTooltip);
  return true;
}

function updateUnauthenticatedPanel(
  root: ShadowRoot,
  strings: FloorpIPProtectionDisclosureStrings,
): boolean {
  const doc = root.ownerDocument;
  if (!doc) {
    return false;
  }
  const title = root.getElementById("unauthenticated-vpn-title");
  const message = root.getElementById("unauthenticated-vpn-message");
  const getStarted = root.getElementById("unauthenticated-get-started");
  const footer = root.getElementById("unauthenticated-footer");
  const terms = root.getElementById("vpn-terms-of-service");
  const privacy = root.getElementById("vpn-privacy-notice");
  if (!title || !message || !getStarted || !footer || !terms || !privacy) {
    return false;
  }

  ensureStyle(root, INNER_STYLE_ID, INNER_SHADOW_STYLES);
  title.removeAttribute("data-l10n-id");
  setTextIfChanged(title, strings.unauthenticatedTitle);

  let disclosure = root.getElementById(FULL_DISCLOSURE_ID);
  if (!disclosure) {
    disclosure = doc.createElement("div");
    disclosure.id = FULL_DISCLOSURE_ID;
    disclosure.setAttribute("role", "note");
    getStarted.before(disclosure);
  }
  setTextIfChanged(
    disclosure,
    `${strings.scope} ${strings.fullDisclosure}`,
  );

  footer.removeAttribute("data-l10n-id");
  terms.removeAttribute("data-l10n-id");
  privacy.removeAttribute("data-l10n-id");
  const expectedFooterText =
    `${strings.termsPrefix}${strings.termsOfUse}${strings.termsConjunction}${strings.privacyNotice}${strings.termsSuffix}`;
  if (
    terms.parentElement !== footer ||
    privacy.parentElement !== footer ||
    terms.textContent !== strings.termsOfUse ||
    privacy.textContent !== strings.privacyNotice ||
    footer.textContent !== expectedFooterText
  ) {
    setTextIfChanged(terms, strings.termsOfUse);
    setTextIfChanged(privacy, strings.privacyNotice);
    footer.replaceChildren(
      doc.createTextNode(strings.termsPrefix),
      terms,
      doc.createTextNode(strings.termsConjunction),
      privacy,
      doc.createTextNode(strings.termsSuffix),
    );
  }
  markReady(footer);
  return true;
}

function cancelPanelRetry(win: BrowserWindow, state: WindowObserver): void {
  if (state.panelRetryFrame !== null) {
    win.cancelAnimationFrame(state.panelRetryFrame);
    state.panelRetryFrame = null;
  }
}

function schedulePanelRetry(
  win: BrowserWindow,
  state: WindowObserver,
  strings: FloorpIPProtectionDisclosureStrings,
): void {
  if (
    state.panelRetryFrame !== null ||
    state.panelRetryAttempts >= MAX_PANEL_RETRY_FRAMES
  ) {
    if (state.panelRetryAttempts === MAX_PANEL_RETRY_FRAMES) {
      state.panelRetryAttempts++;
      console.error(
        "[FloorpIPProtectionUI] Panel did not become ready within the retry window.",
      );
    }
    return;
  }
  state.panelRetryAttempts++;
  state.panelRetryFrame = win.requestAnimationFrame(() => {
    state.panelRetryFrame = null;
    ensurePanel(win, state, strings);
  });
}

function ensurePanelHeaderObserver(
  win: BrowserWindow,
  state: WindowObserver,
  panel: Element,
  strings: FloorpIPProtectionDisclosureStrings,
): void {
  const doc = win.document;
  if (!doc) {
    return;
  }
  const header = doc.getElementById(HEADER_CONTENT_ID);
  const helpButton = doc.getElementById(HEADER_BUTTON_ID);
  if (!header || !helpButton) {
    return;
  }
  if (
    state.panelHeaderNodes?.[0] === header &&
    state.panelHeaderNodes[1] === helpButton
  ) {
    return;
  }
  state.panelHeaderObserver?.disconnect();
  state.panelHeaderNodes = [header, helpButton];
  state.panelHeaderObserver = new win.MutationObserver(() => {
    if (state.panelRepairing) {
      return;
    }
    markNotReady(panel);
    ensurePanel(win, state, strings);
  });
  const options: MutationObserverInit = {
    attributes: true,
    attributeFilter: [
      "data-l10n-id",
      "tooltiptext",
    ],
  };
  state.panelHeaderObserver.observe(header, options);
  state.panelHeaderObserver.observe(helpButton, options);
}

function ensurePanel(
  win: BrowserWindow,
  state: WindowObserver,
  strings: FloorpIPProtectionDisclosureStrings,
): void {
  const doc = win.document;
  if (!doc) {
    return;
  }
  const panel = doc.getElementById(PANEL_ID);
  if (!panel || state.panelRepairing) {
    return;
  }
  state.panelRepairing = true;
  try {
    markNotReady(panel);
    if (!updatePanelHeader(doc, strings)) {
      return;
    }
    ensurePanelHeaderObserver(win, state, panel, strings);
    const contentArea = doc.getElementById(PANEL_CONTENT_ID);
    const content = contentArea?.querySelector("ipprotection-content") as
      | HTMLElement
      | null;
    const outerRoot = content?.shadowRoot;
    if (!outerRoot) {
      if (content) {
        schedulePanelRetry(win, state, strings);
      }
      return;
    }

    if (state.panelOuterRoot !== outerRoot) {
      state.panelOuterObserver?.disconnect();
      state.panelOuterRoot = outerRoot;
      state.panelOuterObserver = new win.MutationObserver(() => {
        if (state.panelRepairing) {
          return;
        }
        markNotReady(panel);
        ensurePanel(win, state, strings);
      });
      state.panelOuterObserver.observe(outerRoot, {
        childList: true,
        subtree: true,
      });
    }

    const wrapper = outerRoot.getElementById("ipprotection-content-wrapper");
    if (!wrapper) {
      schedulePanelRetry(win, state, strings);
      return;
    }
    ensureStyle(outerRoot, OUTER_STYLE_ID, OUTER_SHADOW_STYLES);
    let scope = outerRoot.getElementById(SCOPE_ID);
    if (!scope) {
      const ownerDocument = outerRoot.ownerDocument;
      if (!ownerDocument) {
        return;
      }
      scope = ownerDocument.createElement("div");
      scope.id = SCOPE_ID;
      scope.setAttribute("role", "note");
      wrapper.append(scope);
    }
    setTextIfChanged(scope, strings.scope);

    const unauthenticated = wrapper.querySelector(
      "ipprotection-unauthenticated",
    ) as HTMLElement | null;
    if (unauthenticated) {
      const innerRoot = unauthenticated.shadowRoot;
      if (!innerRoot) {
        schedulePanelRetry(win, state, strings);
        return;
      }
      if (state.panelInnerRoot !== innerRoot) {
        state.panelInnerObserver?.disconnect();
        state.panelInnerRoot = innerRoot;
        state.panelInnerObserver = new win.MutationObserver(() => {
          if (state.panelRepairing) {
            return;
          }
          markNotReady(panel);
          ensurePanel(win, state, strings);
        });
        state.panelInnerObserver.observe(innerRoot, {
          childList: true,
          subtree: true,
        });
      }
      if (!updateUnauthenticatedPanel(innerRoot, strings)) {
        schedulePanelRetry(win, state, strings);
        return;
      }
    } else {
      state.panelInnerObserver?.disconnect();
      state.panelInnerObserver = null;
      state.panelInnerRoot = null;
    }

    cancelPanelRetry(win, state);
    state.panelRetryAttempts = 0;
    markReady(panel);
  } finally {
    win.queueMicrotask(() => {
      state.panelRepairing = false;
    });
  }
}

function ensureToolbar(
  win: BrowserWindow,
  state: WindowObserver,
  strings: FloorpIPProtectionDisclosureStrings,
): void {
  const doc = win.document;
  if (!doc) {
    return;
  }
  const button = doc.getElementById(TOOLBAR_BUTTON_ID);
  if (!button) {
    return;
  }
  if (state.toolbarNode !== button) {
    state.toolbarObserver?.disconnect();
    state.toolbarNode = button;
    state.toolbarObserver = new win.MutationObserver(() => {
      markNotReady(button);
      updateToolbarButton(button, strings);
    });
    state.toolbarObserver.observe(button, {
      attributes: true,
      attributeFilter: [
        "class",
        "data-l10n-id",
        "label",
        "tooltiptext",
        "aria-label",
      ],
    });
  }
  updateToolbarButton(button, strings);
}

function initializeBrowserDocument(win: BrowserWindow): void {
  const doc = win.document;
  const documentElement = doc?.documentElement;
  if (!doc || !documentElement) {
    return;
  }
  if (
    observedWindows.has(win) ||
    documentElement.getAttribute("windowtype") !==
      "navigator:browser"
  ) {
    return;
  }
  const strings = getDisclosureStrings();
  const state: WindowObserver = {
    documentObserver: new win.MutationObserver(() => {
      ensureToolbar(win, state, strings);
      ensurePanel(win, state, strings);
    }),
    toolbarObserver: null,
    panelHeaderObserver: null,
    panelOuterObserver: null,
    panelInnerObserver: null,
    toolbarNode: null,
    panelHeaderNodes: null,
    panelOuterRoot: null,
    panelInnerRoot: null,
    panelRetryFrame: null,
    panelRetryAttempts: 0,
    panelRepairing: false,
  };
  observedWindows.set(win, state);
  state.documentObserver.observe(documentElement, {
    childList: true,
    subtree: true,
  });

  const onPanelEvent = (event: Event): void => {
    const target = event.target as Element | null;
    if (target?.id !== PANEL_ID) {
      return;
    }
    const panel = doc.getElementById(PANEL_ID);
    markNotReady(panel);
    if (event.type === "ViewHiding") {
      cancelPanelRetry(win, state);
      state.panelRetryAttempts = 0;
      return;
    }
    state.panelRetryAttempts = 0;
    ensurePanel(win, state, strings);
  };
  doc.addEventListener("ViewShowing", onPanelEvent, true);
  doc.addEventListener("ViewHiding", onPanelEvent, true);

  win.addEventListener(
    "unload",
    () => {
      state.documentObserver.disconnect();
      state.toolbarObserver?.disconnect();
      state.panelHeaderObserver?.disconnect();
      state.panelOuterObserver?.disconnect();
      state.panelInnerObserver?.disconnect();
      cancelPanelRetry(win, state);
      doc.removeEventListener("ViewShowing", onPanelEvent, true);
      doc.removeEventListener("ViewHiding", onPanelEvent, true);
      observedWindows.delete(win);
    },
    { once: true },
  );

  ensureToolbar(win, state, strings);
  ensurePanel(win, state, strings);
}

function observeBrowserWindow(win: BrowserWindow): void {
  const doc = win.document;
  if (!doc) {
    return;
  }
  if (doc.readyState === "loading") {
    win.addEventListener(
      "DOMContentLoaded",
      () => initializeBrowserDocument(win),
      { once: true },
    );
    return;
  }
  initializeBrowserDocument(win);
}

function installWindowObserver(): void {
  if (windowObserverInstalled) {
    return;
  }
  Services.obs.addObserver((subject) => {
    observeBrowserWindow(subject as BrowserWindow);
  }, "domwindowopened");
  Services.obs.addObserver((subject) => {
    observeBrowserWindow(subject as BrowserWindow);
  }, "browser-delayed-startup-finished");
  const windows = Services.wm.getEnumerator("navigator:browser");
  while (windows.hasMoreElements()) {
    observeBrowserWindow(windows.getNext() as BrowserWindow);
  }
  windowObserverInstalled = true;
}

export const FloorpIPProtectionUI = {
  installEarly(): boolean {
    if (earlyInstallResult !== null) {
      return earlyInstallResult;
    }
    try {
      Services.prefs.setBoolPref(ENABLED_PREF, false);
      Services.prefs.setBoolPref(BLOCK_CALLOUTS_PREF, true);
      registerGuardSheet();
      installCalloutHook();
      installWindowObserver();
      earlyInstallResult = true;
    } catch (error) {
      try {
        Services.prefs.setBoolPref(ENABLED_PREF, false);
      } catch {
        // The adapter must remain fail-closed even if startup is incomplete.
      }
      earlyInstallResult = false;
      console.error(
        "[FloorpIPProtectionUI] Failed to install fail-closed UI adapter:",
        error,
      );
    }
    return earlyInstallResult;
  },
} as const;
