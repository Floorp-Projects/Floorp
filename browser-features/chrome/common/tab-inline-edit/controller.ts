/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import i18next from "i18next";

export const TAB_INLINE_EDIT_PREF = "floorp.tabs.inlineUrlEdit.enabled";
export const TAB_INLINE_EDIT_INPUT_ID = "floorp-tab-inline-url-editor";
export const TAB_INLINE_EDIT_CONTEXT_ITEM_ID =
  "floorp-tab-inline-url-editor-context-item";

const CONTROLLER_OWNER_KEY = "__floorpTabInlineEditController";

function stripAsciiWhitespaceAndControls(value: string): string {
  let stripped = "";
  for (const character of value) {
    const code = character.charCodeAt(0);
    if (code > 0x20 && code !== 0x7f) {
      stripped += character;
    }
  }
  return stripped;
}

export type InlineTab = Element & {
  closing?: boolean;
  linkedPanel?: string;
  multiselected?: boolean;
  focus?: (options?: FocusOptions) => void;
};

export type InlineBrowser = {
  currentURI?: { spec?: string };
  focus?: (options?: FocusOptions) => void;
};

export type InlineGBrowser = {
  tabs?: Iterable<InlineTab> | ArrayLike<InlineTab>;
  selectedTab?: InlineTab | null;
  selectedTabs?: ArrayLike<InlineTab>;
  selectedBrowser?: InlineBrowser | null;
  tabContainer?: EventTarget | null;
  getBrowserForTab: (tab: InlineTab) => InlineBrowser;
};

export type InlineSessionStore = {
  getLazyTabValue: (tab: InlineTab, key: string) => string | undefined;
};

export type InlineUrlFixupResult = {
  url: string;
  postData?: unknown;
};

export type InlineUrlFixup = (
  input: string,
) => Promise<InlineUrlFixupResult>;

export type InlineTabLoadOptions = {
  targetBrowser: InlineBrowser;
  triggeringPrincipal: unknown;
  allowInheritPrincipal: false;
  allowThirdPartyFixup: true;
  indicateErrorPageLoad: true;
  allowPinnedTabHostChange: true;
  postData?: unknown;
};

export interface TabInlineEditControllerDependencies {
  gBrowser: InlineGBrowser | null;
  contextMenu: Element | null;
  overlayParent: Element | null;
  getContextTab: () => InlineTab | null;
  sessionStore: InlineSessionStore | null;
  fixup: InlineUrlFixup;
  openTrustedLinkIn: (
    url: string,
    where: "current",
    options: InlineTabLoadOptions,
  ) => void;
  getSystemPrincipal: () => unknown;
  translate: (key: string, fallback: string) => string;
}

type ControllerWindow = Window & {
  document: Document;
  [CONTROLLER_OWNER_KEY]?: Pick<
    TabInlineEditController,
    "destroy" | "openForSelectedTab" | "updateLocalizedLabels"
  >;
  gBrowser?: InlineGBrowser;
  TabContextMenu?: { contextTab?: InlineTab | null };
  SessionStore?: InlineSessionStore;
  openTrustedLinkIn?: (
    url: string,
    where: "current",
    options: InlineTabLoadOptions,
  ) => void;
};

type EditorState = {
  input: HTMLInputElement;
  targetTab: InlineTab;
  previousFocus: Element | null;
  cleanups: Array<() => void>;
  animationFrame: number | null;
  submitting: boolean;
};

function collectionIncludesTab(
  collection: Iterable<InlineTab> | ArrayLike<InlineTab> | undefined,
  target: InlineTab,
): boolean {
  if (!collection) {
    return true;
  }
  try {
    return Array.from(collection).includes(target);
  } catch {
    return false;
  }
}

export function isDangerousInlineUrl(value: string): boolean {
  const normalized = stripAsciiWhitespaceAndControls(value).toLowerCase();
  return normalized.startsWith("javascript:") || normalized.startsWith("data:");
}

export function normalizeInlineTabPrefill(value: string | undefined): string {
  if (!value || value.toLowerCase() === "about:blank") {
    return "";
  }
  return value;
}

export function getSafeInlineTabPrefill(
  tab: InlineTab,
  gBrowser: InlineGBrowser,
  sessionStore: InlineSessionStore | null,
): string {
  const isLazy = tab.hasAttribute("pending") ||
    tab.hasAttribute("discarded");

  if (isLazy) {
    try {
      return normalizeInlineTabPrefill(
        sessionStore?.getLazyTabValue(tab, "url"),
      );
    } catch {
      return "";
    }
  }

  try {
    return normalizeInlineTabPrefill(
      gBrowser.getBrowserForTab(tab).currentURI?.spec,
    );
  } catch {
    return "";
  }
}

export async function resolveInlineTabUrlInput(
  rawInput: string,
  fixup: InlineUrlFixup,
): Promise<InlineUrlFixupResult | null> {
  const input = rawInput.trim();
  if (!input || isDangerousInlineUrl(input)) {
    return null;
  }

  const fixed = await fixup(input);
  const fixedUrl = fixed.url?.trim();
  if (!fixedUrl || isDangerousInlineUrl(fixedUrl)) {
    return null;
  }

  return { ...fixed, url: fixedUrl };
}

export function buildInlineTabLoadOptions(
  targetBrowser: InlineBrowser,
  triggeringPrincipal: unknown,
  postData?: unknown,
): InlineTabLoadOptions {
  const options: InlineTabLoadOptions = {
    targetBrowser,
    triggeringPrincipal,
    allowInheritPrincipal: false,
    allowThirdPartyFixup: true,
    indicateErrorPageLoad: true,
    allowPinnedTabHostChange: true,
  };
  if (postData !== undefined && postData !== null) {
    options.postData = postData;
  }
  return options;
}

function defaultFixup(input: string): Promise<InlineUrlFixupResult> {
  const imported = ChromeUtils.importESModule(
    "moz-src:///browser/components/urlbar/UrlbarUtils.sys.mjs",
  ) as {
    UrlbarUtils: {
      getShortcutOrURIAndPostData: (
        value: string,
      ) => Promise<InlineUrlFixupResult>;
    };
  };
  return imported.UrlbarUtils.getShortcutOrURIAndPostData(input);
}

function defaultTranslate(key: string, fallback: string): string {
  return i18next.t(key, { defaultValue: fallback });
}

function createDefaultDependencies(
  win: ControllerWindow,
): TabInlineEditControllerDependencies {
  return {
    gBrowser: win.gBrowser ?? null,
    contextMenu: win.document.getElementById("tabContextMenu"),
    overlayParent: win.document.documentElement,
    getContextTab: () => win.TabContextMenu?.contextTab ?? null,
    sessionStore: win.SessionStore ?? null,
    fixup: defaultFixup,
    openTrustedLinkIn: (url, where, options) => {
      if (typeof win.openTrustedLinkIn !== "function") {
        throw new Error("openTrustedLinkIn is unavailable");
      }
      win.openTrustedLinkIn(url, where, options);
    },
    getSystemPrincipal: () =>
      Services.scriptSecurityManager.getSystemPrincipal(),
    translate: defaultTranslate,
  };
}

function mergeDependencies(
  defaults: TabInlineEditControllerDependencies,
  overrides: Partial<TabInlineEditControllerDependencies>,
): TabInlineEditControllerDependencies {
  return {
    ...defaults,
    ...overrides,
  };
}

export class TabInlineEditController {
  private readonly win: ControllerWindow;
  private readonly deps: TabInlineEditControllerDependencies;
  private contextItem: Element | null = null;
  private editor: EditorState | null = null;
  private destroyed = false;
  private navigationEpoch = 0;
  private waitingForDOMContentLoaded = false;

  private readonly handleWindowUnload = () => this.destroy();
  private readonly handleDOMContentLoaded = () => {
    this.waitingForDOMContentLoaded = false;
    if (!this.deps.contextMenu) {
      this.deps.contextMenu = this.win.document.getElementById(
        "tabContextMenu",
      );
    }
    this.installContextItem();
  };
  private readonly handleContextMenuShowing = (event: Event) => {
    if (event.target !== this.deps.contextMenu) {
      return;
    }
    this.updateContextItemState();
  };
  private readonly handleContextCommand = () => {
    this.openForContextTab();
  };

  constructor(
    win: Window = globalThis as unknown as Window,
    dependencies: Partial<TabInlineEditControllerDependencies> = {},
  ) {
    this.win = win as ControllerWindow;

    const existing = this.win[CONTROLLER_OWNER_KEY];
    if (existing && existing !== this) {
      existing.destroy();
    }

    this.deps = mergeDependencies(
      createDefaultDependencies(this.win),
      dependencies,
    );
    this.win[CONTROLLER_OWNER_KEY] = this;

    this.win.addEventListener("unload", this.handleWindowUnload, {
      once: true,
    });
    this.installContextItem();
  }

  public openForContextTab(): boolean {
    return this.openForTab(this.deps.getContextTab());
  }

  public openForSelectedTab(): boolean {
    return this.openForTab(this.deps.gBrowser?.selectedTab ?? null);
  }

  public openForTab(tab: InlineTab | null | undefined): boolean {
    if (this.destroyed || !tab || !this.canEditTab(tab)) {
      return false;
    }

    if (this.editor?.targetTab === tab) {
      this.editor.input.focus({ preventScroll: true });
      this.editor.input.select();
      return true;
    }

    this.navigationEpoch++;
    const previousFocus = this.editor?.previousFocus ??
      this.win.document.activeElement;
    this.closeEditor(false);

    const input = this.createInput();
    const parent = this.deps.overlayParent;
    const gBrowser = this.deps.gBrowser;
    if (!input || !parent || !gBrowser) {
      return false;
    }

    input.value = getSafeInlineTabPrefill(
      tab,
      gBrowser,
      this.deps.sessionStore,
    );
    parent.appendChild(input);

    this.editor = {
      input,
      targetTab: tab,
      previousFocus,
      cleanups: [],
      animationFrame: null,
      submitting: false,
    };
    this.installEditorListeners(this.editor);
    this.repositionEditor();

    try {
      input.focus({ preventScroll: true });
    } catch {
      input.focus();
    }
    input.select();
    return true;
  }

  public updateLocalizedLabels(): void {
    const menuLabel = this.deps.translate(
      "tabInlineEdit.contextMenu",
      "Edit Tab URL",
    );
    this.contextItem?.setAttribute("label", menuLabel);

    if (this.editor) {
      const inputLabel = this.deps.translate(
        "tabInlineEdit.inputLabel",
        "Tab URL",
      );
      this.editor.input.setAttribute("aria-label", inputLabel);
      this.editor.input.setAttribute("placeholder", inputLabel);
    }
  }

  public destroy(): void {
    if (this.destroyed) {
      return;
    }
    this.destroyed = true;
    this.navigationEpoch++;

    this.closeEditor(true);
    this.win.removeEventListener("unload", this.handleWindowUnload);

    if (this.waitingForDOMContentLoaded) {
      this.win.document.removeEventListener(
        "DOMContentLoaded",
        this.handleDOMContentLoaded,
      );
      this.waitingForDOMContentLoaded = false;
    }

    this.deps.contextMenu?.removeEventListener(
      "popupshowing",
      this.handleContextMenuShowing,
    );
    this.contextItem?.removeEventListener(
      "command",
      this.handleContextCommand,
    );
    this.contextItem?.remove();
    this.contextItem = null;

    if (this.win[CONTROLLER_OWNER_KEY] === this) {
      delete this.win[CONTROLLER_OWNER_KEY];
    }
  }

  private installContextItem(): void {
    if (this.destroyed || this.contextItem) {
      return;
    }

    const menu = this.deps.contextMenu;
    if (!menu) {
      if (
        this.win.document.readyState === "loading" &&
        !this.waitingForDOMContentLoaded
      ) {
        this.waitingForDOMContentLoaded = true;
        this.win.document.addEventListener(
          "DOMContentLoaded",
          this.handleDOMContentLoaded,
          { once: true },
        );
      }
      return;
    }

    this.win.document.getElementById(TAB_INLINE_EDIT_CONTEXT_ITEM_ID)?.remove();
    const chromeDocument = this.win.document as Document & {
      createXULElement?: (tagName: string) => Element;
    };
    const item = chromeDocument.createXULElement?.("menuitem") ??
      this.win.document.createElement("menuitem");
    item.id = TAB_INLINE_EDIT_CONTEXT_ITEM_ID;
    item.addEventListener("command", this.handleContextCommand);

    const marker = this.win.document.getElementById("context_duplicateTab") ??
      this.win.document.getElementById("context_reloadTab");
    if (marker?.parentElement === menu) {
      menu.insertBefore(item, marker);
    } else {
      menu.appendChild(item);
    }

    this.contextItem = item;
    menu.addEventListener("popupshowing", this.handleContextMenuShowing);
    this.updateLocalizedLabels();
    this.updateContextItemState();
  }

  private updateContextItemState(): void {
    if (!this.contextItem) {
      return;
    }
    if (this.canEditTab(this.deps.getContextTab())) {
      this.contextItem.removeAttribute("disabled");
    } else {
      this.contextItem.setAttribute("disabled", "true");
    }
  }

  private canEditTab(tab: InlineTab | null | undefined): tab is InlineTab {
    const gBrowser = this.deps.gBrowser;
    if (!tab || !gBrowser || tab.closing || tab.hasAttribute("closing")) {
      return false;
    }
    if (!tab.isConnected || !collectionIncludesTab(gBrowser.tabs, tab)) {
      return false;
    }
    if (
      tab.multiselected ||
      tab.hasAttribute("multiselected") ||
      (gBrowser.selectedTabs?.length ?? 0) > 1
    ) {
      return false;
    }
    return true;
  }

  private createInput(): HTMLInputElement {
    this.win.document.getElementById(TAB_INLINE_EDIT_INPUT_ID)?.remove();
    const input = this.win.document.createElementNS(
      "http://www.w3.org/1999/xhtml",
      "input",
    ) as HTMLInputElement;
    input.id = TAB_INLINE_EDIT_INPUT_ID;
    input.type = "text";
    input.autocomplete = "off";
    input.spellcheck = false;
    input.setAttribute("role", "textbox");
    input.style.cssText = [
      "position: fixed",
      "z-index: 2147483647",
      "box-sizing: border-box",
      "margin: 0",
      "padding: 2px 8px",
      "border: 1px solid var(--focus-outline-color, AccentColor)",
      "border-radius: 4px",
      "background: var(--toolbar-field-background-color, Field)",
      "color: var(--toolbar-field-color, FieldText)",
      "font: menu",
      "box-shadow: 0 2px 8px rgba(0, 0, 0, 0.35)",
    ].join(";");
    this.updateInputLocalizedLabel(input);
    return input;
  }

  private updateInputLocalizedLabel(input: HTMLInputElement): void {
    const label = this.deps.translate(
      "tabInlineEdit.inputLabel",
      "Tab URL",
    );
    input.setAttribute("aria-label", label);
    input.setAttribute("placeholder", label);
  }

  private installEditorListeners(state: EditorState): void {
    const listen = (
      target: EventTarget,
      type: string,
      listener: EventListener,
      options?: boolean | AddEventListenerOptions,
    ) => {
      target.addEventListener(type, listener, options);
      state.cleanups.push(() =>
        target.removeEventListener(type, listener, options)
      );
    };

    const onKeyDown = (event: Event) => {
      const keyboardEvent = event as KeyboardEvent;
      if (keyboardEvent.key === "Enter") {
        keyboardEvent.preventDefault();
        keyboardEvent.stopPropagation();
        this.submitEditor();
      } else if (keyboardEvent.key === "Escape") {
        keyboardEvent.preventDefault();
        keyboardEvent.stopPropagation();
        this.navigationEpoch++;
        this.closeEditor(true);
      }
    };
    const onBlur = () => {
      if (!state.submitting) {
        this.navigationEpoch++;
        this.closeEditor(true);
      }
    };
    const onReposition = () => this.scheduleReposition();
    const onTabEvent = (event: Event) => {
      if (event.type === "TabClose" && event.target === state.targetTab) {
        this.navigationEpoch++;
        this.closeEditor(true);
        return;
      }
      this.scheduleReposition();
    };

    listen(state.input, "keydown", onKeyDown);
    listen(state.input, "blur", onBlur);
    listen(this.win, "resize", onReposition);
    listen(this.win, "scroll", onReposition, true);

    const tabContainer = this.deps.gBrowser?.tabContainer;
    if (tabContainer) {
      listen(tabContainer, "scroll", onReposition, true);
      for (
        const eventName of [
          "TabMove",
          "TabClose",
          "TabPinned",
          "TabUnpinned",
          "TabAttrModified",
          "TabGroupCollapse",
          "TabGroupExpand",
        ]
      ) {
        listen(tabContainer, eventName, onTabEvent);
      }
    }

    const resizeObserverConstructor = (
      this.win as Window & { ResizeObserver?: typeof ResizeObserver }
    ).ResizeObserver;
    if (resizeObserverConstructor) {
      const observer = new resizeObserverConstructor(onReposition);
      observer.observe(state.targetTab);
      if (tabContainer instanceof Element) {
        observer.observe(tabContainer);
      }
      state.cleanups.push(() => observer.disconnect());
    }

    const mutationObserverConstructor = (
      this.win as Window & { MutationObserver?: typeof MutationObserver }
    ).MutationObserver;
    if (mutationObserverConstructor) {
      const observer = new mutationObserverConstructor(() => {
        if (!state.targetTab.isConnected) {
          this.navigationEpoch++;
          this.closeEditor(true);
          return;
        }
        this.scheduleReposition();
      });

      if (tabContainer instanceof Node) {
        observer.observe(tabContainer, {
          attributes: true,
          childList: true,
          subtree: true,
          attributeFilter: [
            "class",
            "collapsed",
            "hidden",
            "orient",
            "style",
            "vertical",
          ],
        });
      }

      let ancestor = state.targetTab.parentElement;
      while (ancestor && ancestor !== tabContainer) {
        observer.observe(ancestor, {
          attributes: true,
          attributeFilter: [
            "class",
            "collapsed",
            "hidden",
            "orient",
            "style",
            "vertical",
          ],
        });
        ancestor = ancestor.parentElement;
      }
      state.cleanups.push(() => observer.disconnect());
    }
  }

  private scheduleReposition(): void {
    const state = this.editor;
    if (!state || state.animationFrame !== null) {
      return;
    }
    state.animationFrame = this.win.requestAnimationFrame(() => {
      if (this.editor === state) {
        state.animationFrame = null;
        this.repositionEditor();
      }
    });
  }

  private repositionEditor(): void {
    const state = this.editor;
    if (!state) {
      return;
    }
    if (!state.targetTab.isConnected || !state.input.isConnected) {
      this.navigationEpoch++;
      this.closeEditor(true);
      return;
    }

    const rect = state.targetTab.getBoundingClientRect();
    const viewportWidth = Math.max(
      this.win.innerWidth,
      this.win.document.documentElement?.clientWidth ?? 0,
      1,
    );
    const viewportHeight = Math.max(
      this.win.innerHeight,
      this.win.document.documentElement?.clientHeight ?? 0,
      1,
    );
    const inset = 4;
    const width = Math.min(
      Math.max(rect.width, 240),
      Math.max(1, viewportWidth - inset * 2),
    );
    const height = Math.min(
      Math.max(rect.height, 28),
      Math.max(1, viewportHeight - inset * 2),
    );
    const left = Math.min(
      Math.max(rect.left, inset),
      Math.max(inset, viewportWidth - width - inset),
    );
    const top = Math.min(
      Math.max(rect.top, inset),
      Math.max(inset, viewportHeight - height - inset),
    );

    state.input.style.left = `${Math.round(left)}px`;
    state.input.style.top = `${Math.round(top)}px`;
    state.input.style.width = `${Math.round(width)}px`;
    state.input.style.height = `${Math.round(height)}px`;
  }

  private submitEditor(): void {
    const state = this.editor;
    if (!state || state.submitting) {
      return;
    }
    state.submitting = true;
    const rawInput = state.input.value;
    const targetTab = state.targetTab;
    const epoch = ++this.navigationEpoch;
    this.closeEditor(true);
    void this.navigateFromInput(rawInput, targetTab, epoch);
  }

  private async navigateFromInput(
    rawInput: string,
    targetTab: InlineTab,
    epoch: number,
  ): Promise<void> {
    try {
      const fixed = await resolveInlineTabUrlInput(rawInput, this.deps.fixup);
      if (
        !fixed ||
        this.destroyed ||
        epoch !== this.navigationEpoch ||
        !this.canEditTab(targetTab)
      ) {
        return;
      }

      const gBrowser = this.deps.gBrowser;
      if (!gBrowser) {
        return;
      }
      const targetBrowser = gBrowser.getBrowserForTab(targetTab);
      const options = buildInlineTabLoadOptions(
        targetBrowser,
        this.deps.getSystemPrincipal(),
        fixed.postData,
      );
      this.deps.openTrustedLinkIn(fixed.url, "current", options);
    } catch (error) {
      console.error(
        "[TabInlineEdit] Failed to navigate edited tab URL:",
        error,
      );
    }
  }

  private closeEditor(restoreFocus: boolean): void {
    const state = this.editor;
    if (!state) {
      return;
    }
    this.editor = null;

    if (state.animationFrame !== null) {
      this.win.cancelAnimationFrame(state.animationFrame);
      state.animationFrame = null;
    }
    for (const cleanup of state.cleanups.splice(0).reverse()) {
      try {
        cleanup();
      } catch {
        // The window or tab may already be closing.
      }
    }
    state.input.remove();

    if (restoreFocus) {
      this.restoreFocus(state.previousFocus, state.targetTab);
    }
  }

  private restoreFocus(
    previousFocus: Element | null,
    targetTab: InlineTab,
  ): void {
    if (this.focusIfValid(previousFocus)) {
      return;
    }
    if (this.focusIfValid(targetTab)) {
      return;
    }
    this.focusIfValid(this.deps.gBrowser?.selectedBrowser ?? null);
  }

  private focusIfValid(candidate: Element | InlineBrowser | null): boolean {
    const focusable = candidate as {
      focus?: (options?: FocusOptions) => void;
    } | null;
    if (!focusable || typeof focusable.focus !== "function") {
      return false;
    }

    if (candidate instanceof Element) {
      if (
        !candidate.isConnected ||
        candidate.hasAttribute("disabled") ||
        candidate.hasAttribute("hidden") ||
        candidate.getClientRects().length === 0
      ) {
        return false;
      }
    }

    try {
      focusable.focus({ preventScroll: true });
    } catch {
      try {
        focusable.focus();
      } catch {
        return false;
      }
    }
    return true;
  }
}

export function openInlineTabUrlEditorForSelectedTab(win: Window): boolean {
  const controller = (win as ControllerWindow)[CONTROLLER_OWNER_KEY];
  return controller?.openForSelectedTab() ?? false;
}
