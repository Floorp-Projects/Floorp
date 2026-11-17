// SPDX-License-Identifier: MPL-2.0

import type {
  PreserveLoadBrowser,
  PreserveLoadChannel,
  PreserveLoadDocument,
  PreserveLoadProgressListener,
  PreserveLoadRequest,
  PreserveLoadTab,
  PreserveLoadTabBrowser,
  PreserveLoadURI,
  PreserveLoadUrlbar,
  PreserveLoadWebProgress,
  TabActiveLoadState,
  TabLoadPreserveControllerOptions,
  TabLoadRecord,
} from "./types.ts";

const CONTROLLER_SLOT = "__floorpTabLoadPreserveController__" as const;

const STATE_START = 0x00000001;
const STATE_STOP = 0x00000010;
const STATE_IS_NETWORK = 0x00040000;

const LOCATION_CHANGE_SAME_DOCUMENT = 0x00000001;
const LOCATION_CHANGE_ERROR_PAGE = 0x00000004;

const LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP = 0x100000;
const LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL = 0x40000;
const LOAD_FLAGS_BYPASS_CACHE = 0x0100;
const LOAD_FLAGS_BYPASS_PROXY = 0x0200;
const LOAD_FLAGS_FIXUP_SCHEME_TYPOS = 0x200000;

function getWebNavigationLoadFlags() {
  const nav =
    typeof Ci !== "undefined" ? Ci.nsIWebNavigation : undefined;
  return {
    allowThirdPartyFixup:
      nav?.LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP ??
      LOAD_FLAGS_ALLOW_THIRD_PARTY_FIXUP,
    disallowInheritPrincipal:
      nav?.LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL ??
      LOAD_FLAGS_DISALLOW_INHERIT_PRINCIPAL,
    bypassCache: nav?.LOAD_FLAGS_BYPASS_CACHE ?? LOAD_FLAGS_BYPASS_CACHE,
    bypassProxy: nav?.LOAD_FLAGS_BYPASS_PROXY ?? LOAD_FLAGS_BYPASS_PROXY,
    fixupSchemeTypos:
      nav?.LOAD_FLAGS_FIXUP_SCHEME_TYPOS ?? LOAD_FLAGS_FIXUP_SCHEME_TYPOS,
  };
}

const DEFAULT_CANCELLED_OLD_LOAD_STATUS = 0x805d0027;
const HAS_SCHEME_RE = /^[a-zA-Z][a-zA-Z\d+.-]*:/;

const KNOWN_BLANK_OR_INITIAL_BASES = new Set([
  "about:blank",
  "about:home",
  "about:newtab",
  "about:privatebrowsing",
  "about:opentabs",
  "about:firefoxview",
  "about:sessionrestore",
  "about:welcome",
  "about:welcomeback",
  "chrome://browser/content/blanktab.html",
]);

type ControllerHost = {
  [CONTROLLER_SLOT]?: TabLoadPreserveController;
};

type WrappedMethods = {
  reload?: () => void;
  reloadTab?: (tab: PreserveLoadTab) => void;
  reloadTabs?: (tabs: readonly PreserveLoadTab[]) => void;
  reloadWithFlags?: (reloadFlags: number) => void;
};

function defaultIsSuccessCode(status: number): boolean {
  if (typeof Components !== "undefined" && Components.isSuccessCode) {
    return Components.isSuccessCode(status);
  }
  return (status >>> 0) < 0x80000000;
}

function defaultCancelledOldLoadStatus(): number {
  if (typeof Cr !== "undefined" && typeof Cr.NS_BINDING_CANCELLED_OLD_LOAD === "number") {
    return Cr.NS_BINDING_CANCELLED_OLD_LOAD;
  }
  return DEFAULT_CANCELLED_OLD_LOAD_STATUS;
}

function defaultGetSystemPrincipal(): unknown {
  if (typeof Services !== "undefined" && Services.scriptSecurityManager) {
    return Services.scriptSecurityManager.getSystemPrincipal();
  }
  return undefined;
}

export function stripQueryAndHash(url: string): string {
  const hashIdx = url.indexOf("#");
  const withoutHash = hashIdx === -1 ? url : url.slice(0, hashIdx);
  const queryIdx = withoutHash.indexOf("?");
  return queryIdx === -1 ? withoutHash : withoutHash.slice(0, queryIdx);
}

export function isBlockedCaptureUrl(url: string): boolean {
  const trimmed = url.trim().toLowerCase();
  if (!trimmed) {
    return true;
  }
  return (
    trimmed.startsWith("javascript:") ||
    trimmed.startsWith("wyciwyg:") ||
    trimmed.startsWith("about:neterror") ||
    trimmed.startsWith("about:certerror") ||
    trimmed.startsWith("about:blocked")
  );
}

export function extractRequestUrl(
  request: PreserveLoadRequest | null | undefined,
): string | null {
  if (!request) {
    return null;
  }

  let channel: PreserveLoadChannel = request;
  if (
    typeof Ci !== "undefined" &&
    Ci.nsIChannel &&
    typeof request.QueryInterface === "function"
  ) {
    try {
      channel = request.QueryInterface(Ci.nsIChannel);
    } catch {
      channel = request;
    }
  }

  const originalSpec = channel.originalURI?.spec?.trim();
  if (originalSpec) {
    return originalSpec;
  }

  const uriSpec = channel.URI?.spec?.trim();
  return uriSpec || null;
}

export class TabLoadPreserveController {
  private readonly tabBrowser: PreserveLoadTabBrowser;
  private readonly urlbar: PreserveLoadUrlbar | null;
  private readonly doc: PreserveLoadDocument | null;
  private readonly isBlankPageURLFn?: (url: string) => boolean;
  private readonly isInitialPageFn?: (url: string) => boolean;
  private readonly isSuccessCodeFn: (status: number) => boolean;
  private readonly getSystemPrincipalFn: () => unknown;
  private readonly cancelledOldLoadStatus: number;

  private readonly tabRecords = new WeakMap<PreserveLoadTab, TabLoadRecord>();
  private readonly originalMethods: WrappedMethods = {};
  private started = false;

  private readonly progressListener: PreserveLoadProgressListener = {
    onStateChange: (browser, webProgress, request, stateFlags, status) => {
      this.handleStateChange(browser, webProgress, request, stateFlags, status);
    },
    onLocationChange: (browser, webProgress, _request, location, flags) => {
      this.handleLocationChange(browser, webProgress, location, flags);
    },
  };

  private readonly onTabSelect = (event: Event): void => {
    const target = event.target as PreserveLoadTab | null;
    const tab = target ?? this.tabBrowser.selectedTab;
    if (tab) {
      this.syncReloadCommand(tab);
    }
  };

  private readonly onTabClose = (event: Event): void => {
    const tab = event.target as PreserveLoadTab | null;
    if (tab) {
      this.tabRecords.delete(tab);
    }
  };

  constructor(options: TabLoadPreserveControllerOptions) {
    this.tabBrowser = options.tabBrowser;
    this.urlbar = options.urlbar;
    this.doc = options.document;
    this.isBlankPageURLFn = options.isBlankPageURL;
    this.isInitialPageFn = options.isInitialPage;
    this.isSuccessCodeFn = options.isSuccessCode ?? defaultIsSuccessCode;
    this.getSystemPrincipalFn =
      options.getSystemPrincipal ?? defaultGetSystemPrincipal;
    this.cancelledOldLoadStatus =
      options.cancelledOldLoadStatus ?? defaultCancelledOldLoadStatus();
  }

  start(): void {
    if (this.started) {
      return;
    }
    this.started = true;
    this.tabBrowser.addTabsProgressListener(this.progressListener);
    this.tabBrowser.tabContainer.addEventListener("TabSelect", this.onTabSelect);
    this.tabBrowser.tabContainer.addEventListener("TabClose", this.onTabClose);
    this.installReloadWrappers();
  }

  destroy(): void {
    if (!this.started) {
      return;
    }
    this.started = false;
    this.uninstallReloadWrappers();
    try {
      this.tabBrowser.removeTabsProgressListener(this.progressListener);
    } catch (error) {
      console.error(
        "[tab-load-preserve] Failed to remove tabs progress listener:",
        error,
      );
    }
    this.tabBrowser.tabContainer.removeEventListener(
      "TabSelect",
      this.onTabSelect,
    );
    this.tabBrowser.tabContainer.removeEventListener(
      "TabClose",
      this.onTabClose,
    );
  }

  isBlankOrInitialUrl(url: string): boolean {
    const trimmed = url.trim();
    if (!trimmed) {
      return true;
    }

    const base = stripQueryAndHash(trimmed).toLowerCase();
    if (KNOWN_BLANK_OR_INITIAL_BASES.has(base)) {
      return true;
    }

    try {
      if (this.isBlankPageURLFn?.(trimmed)) {
        return true;
      }
    } catch {
      // Ignore invalid URL checks from host helpers.
    }

    try {
      if (this.isInitialPageFn?.(trimmed)) {
        return true;
      }
    } catch {
      // Ignore invalid URL checks from host helpers.
    }

    return false;
  }

  getPreservedUrl(tab: PreserveLoadTab): string | null {
    return this.tabRecords.get(tab)?.preservedUrl ?? null;
  }

  private getOrCreateRecord(tab: PreserveLoadTab): TabLoadRecord {
    let record = this.tabRecords.get(tab);
    if (!record) {
      record = { activeLoad: null, preservedUrl: null };
      this.tabRecords.set(tab, record);
    }
    return record;
  }

  private handleStateChange(
    browser: PreserveLoadBrowser,
    webProgress: PreserveLoadWebProgress | null | undefined,
    request: PreserveLoadRequest | null | undefined,
    stateFlags: number,
    status: number,
  ): void {
    if (!webProgress?.isTopLevel) {
      return;
    }

    const tab = this.tabBrowser.getTabForBrowser(browser);
    if (!tab || tab.closing) {
      return;
    }

    const isNetwork = (stateFlags & STATE_IS_NETWORK) !== 0;
    if (!isNetwork) {
      return;
    }

    if ((stateFlags & STATE_START) !== 0) {
      this.onNetworkStart(tab, browser, request);
    } else if ((stateFlags & STATE_STOP) !== 0) {
      this.onNetworkStop(tab, browser, request, status);
    }
  }

  private onNetworkStart(
    tab: PreserveLoadTab,
    browser: PreserveLoadBrowser,
    request: PreserveLoadRequest | null | undefined,
  ): void {
    const record = this.getOrCreateRecord(tab);
    const currentSpec = browser.currentURI?.spec ?? "";
    const fromBlankOrInitial = this.isBlankOrInitialUrl(currentSpec);

    const rawUserTyped = browser.userTypedValue;
    const userTyped =
      typeof rawUserTyped === "string" && rawUserTyped.trim() !== ""
        ? rawUserTyped
        : null;

    const requestUrl = extractRequestUrl(request);

    let candidateUrl: string | null = null;
    let fromUserTyped = false;

    if (userTyped !== null) {
      if (!this.isBlankOrInitialUrl(userTyped)) {
        candidateUrl = userTyped;
        fromUserTyped = true;
      }
    } else if (
      fromBlankOrInitial &&
      requestUrl !== null &&
      !this.isBlankOrInitialUrl(requestUrl) &&
      !isBlockedCaptureUrl(requestUrl)
    ) {
      candidateUrl = requestUrl;
      fromUserTyped = false;
    }

    if (candidateUrl !== null) {
      record.activeLoad = {
        url: candidateUrl,
        fromUserTyped,
        fromBlankOrInitial,
        committed: false,
      };
      record.preservedUrl = null;
    } else {
      record.activeLoad = null;
      record.preservedUrl = null;
    }
  }

  private onNetworkStop(
    tab: PreserveLoadTab,
    browser: PreserveLoadBrowser,
    request: PreserveLoadRequest | null | undefined,
    status: number,
  ): void {
    const record = this.tabRecords.get(tab);

    if (this.isSuccessCodeFn(status)) {
      if (record?.activeLoad?.committed) {
        record.activeLoad = null;
        record.preservedUrl = null;
      } else if (record?.activeLoad) {
        record.activeLoad = null;
      }
      return;
    }

    if (status === this.cancelledOldLoadStatus || browser.isNavigating) {
      return;
    }

    const currentSpec = browser.currentURI?.spec ?? "";
    const stillBlankOrInitial = this.isBlankOrInitialUrl(currentSpec);

    let activeLoad: TabActiveLoadState | null = record?.activeLoad ?? null;
    if (!activeLoad && stillBlankOrInitial) {
      const fallbackUrl = extractRequestUrl(request);
      if (
        fallbackUrl !== null &&
        !this.isBlankOrInitialUrl(fallbackUrl) &&
        !isBlockedCaptureUrl(fallbackUrl)
      ) {
        activeLoad = {
          url: fallbackUrl,
          fromUserTyped: false,
          fromBlankOrInitial: true,
          committed: false,
        };
      }
    }

    if (!activeLoad || activeLoad.committed) {
      return;
    }

    if (!activeLoad.fromUserTyped && !stillBlankOrInitial) {
      if (record) {
        record.activeLoad = null;
      }
      return;
    }

    // If the user started typing something different into the URL bar while
    // this load was still in flight, do not clobber their new input.
    const startedLoadSinceTyping =
      browser.didStartLoadSinceLastUserTyping?.() ?? true;
    const currentTyped = browser.userTypedValue;
    if (
      !startedLoadSinceTyping &&
      typeof currentTyped === "string" &&
      currentTyped !== activeLoad.url
    ) {
      if (record) {
        record.activeLoad = null;
      }
      return;
    }

    const targetRecord = record ?? this.getOrCreateRecord(tab);
    targetRecord.activeLoad = null;

    browser.userTypedValue = activeLoad.url;

    if (stillBlankOrInitial) {
      targetRecord.preservedUrl = activeLoad.url;
      try {
        this.tabBrowser.setInitialTabTitle?.(tab, activeLoad.url, {
          isURL: HAS_SCHEME_RE.test(activeLoad.url),
        });
      } catch (error) {
        console.error(
          "[tab-load-preserve] Failed to set initial tab title:",
          error,
        );
      }
    } else {
      targetRecord.preservedUrl = null;
    }

    if (tab.selected) {
      try {
        this.urlbar?.setURI();
      } catch (error) {
        console.error("[tab-load-preserve] Failed to update URL bar:", error);
      }
      this.syncReloadCommand(tab);
    }
  }

  private handleLocationChange(
    browser: PreserveLoadBrowser,
    webProgress: PreserveLoadWebProgress | null | undefined,
    location: PreserveLoadURI | null | undefined,
    flags: number,
  ): void {
    if (!webProgress?.isTopLevel) {
      return;
    }

    const tab = this.tabBrowser.getTabForBrowser(browser);
    if (!tab) {
      return;
    }

    const isSameDocument = (flags & LOCATION_CHANGE_SAME_DOCUMENT) !== 0;
    if (isSameDocument) {
      return;
    }

    const spec = location?.spec ?? "";
    const isErrorPage = (flags & LOCATION_CHANGE_ERROR_PAGE) !== 0;
    if (isErrorPage && stripQueryAndHash(spec) === "about:blank") {
      return;
    }

    const record = this.tabRecords.get(tab);
    if (!this.isBlankOrInitialUrl(spec)) {
      if (record) {
        if (record.activeLoad) {
          record.activeLoad.committed = true;
        }
        record.preservedUrl = null;
      }
      return;
    }

    if (tab.selected) {
      this.syncReloadCommand(tab);
    }
  }

  private syncReloadCommand(tab: PreserveLoadTab): void {
    const record = this.tabRecords.get(tab);
    if (!record?.preservedUrl) {
      return;
    }

    const browser = tab.linkedBrowser;
    if (
      !browser ||
      !this.isBlankOrInitialUrl(browser.currentURI?.spec ?? "") ||
      !browser.userTypedValue?.trim()
    ) {
      return;
    }

    const reloadCommand = this.doc?.getElementById("Browser:Reload");
    reloadCommand?.removeAttribute("disabled");
  }

  private tryLoadPreservedUrl(tab: PreserveLoadTab, reloadFlags: number): boolean {
    const record = this.tabRecords.get(tab);
    const browser = tab.linkedBrowser;
    if (!record?.preservedUrl || !browser?.fixupAndLoadURIString) {
      return false;
    }

    if (!this.isBlankOrInitialUrl(browser.currentURI?.spec ?? "")) {
      record.preservedUrl = null;
      return false;
    }

    const typed = browser.userTypedValue;
    if (typeof typed !== "string" || typed.trim() === "") {
      record.preservedUrl = null;
      return false;
    }

    const targetUrl = typed;
    record.preservedUrl = null;
    record.activeLoad = null;

    const flags = getWebNavigationLoadFlags();
    const effectiveFlags =
      flags.allowThirdPartyFixup |
      flags.fixupSchemeTypos |
      flags.disallowInheritPrincipal |
      (reloadFlags & (flags.bypassCache | flags.bypassProxy));

    try {
      browser.userTypedValue = targetUrl;
      browser.fixupAndLoadURIString(targetUrl, {
        loadFlags: effectiveFlags,
        triggeringPrincipal: this.getSystemPrincipalFn(),
      });
      return true;
    } catch (error) {
      record.preservedUrl = targetUrl;
      console.error(
        "[tab-load-preserve] Failed to retry preserved URL on reload:",
        error,
      );
      return false;
    }
  }

  private installReloadWrappers(): void {
    if (typeof this.tabBrowser.reloadWithFlags === "function") {
      const originalReloadWithFlags = this.tabBrowser.reloadWithFlags;
      this.originalMethods.reloadWithFlags = originalReloadWithFlags;
      this.tabBrowser.reloadWithFlags = (reloadFlags: number) => {
        const selectedTabs =
          this.tabBrowser.selectedTabs && this.tabBrowser.selectedTabs.length > 0
            ? this.tabBrowser.selectedTabs
            : this.tabBrowser.selectedTab
            ? [this.tabBrowser.selectedTab]
            : [];

        if (selectedTabs.length === 1) {
          if (this.tryLoadPreservedUrl(selectedTabs[0], reloadFlags)) {
            return;
          }
          originalReloadWithFlags.call(this.tabBrowser, reloadFlags);
          return;
        }

        const remaining: PreserveLoadTab[] = [];
        let handledAny = false;
        for (const tab of selectedTabs) {
          if (this.tryLoadPreservedUrl(tab, reloadFlags)) {
            handledAny = true;
          } else {
            remaining.push(tab);
          }
        }
        if (remaining.length === 0) {
          return;
        }
        if (!handledAny) {
          originalReloadWithFlags.call(this.tabBrowser, reloadFlags);
          return;
        }
        for (const tab of remaining) {
          if (typeof tab.linkedBrowser?.reloadWithFlags === "function") {
            tab.linkedBrowser.reloadWithFlags(reloadFlags);
          } else {
            this.originalMethods.reloadTab?.call(this.tabBrowser, tab);
          }
        }
      };
    }

    if (typeof this.tabBrowser.reloadTab === "function") {
      const originalReloadTab = this.tabBrowser.reloadTab;
      this.originalMethods.reloadTab = originalReloadTab;
      this.tabBrowser.reloadTab = (tab: PreserveLoadTab) => {
        if (this.tryLoadPreservedUrl(tab, 0)) {
          return;
        }
        originalReloadTab.call(this.tabBrowser, tab);
      };
    }

    if (typeof this.tabBrowser.reloadTabs === "function") {
      const originalReloadTabs = this.tabBrowser.reloadTabs;
      this.originalMethods.reloadTabs = originalReloadTabs;
      this.tabBrowser.reloadTabs = (tabs: readonly PreserveLoadTab[]) => {
        const remaining: PreserveLoadTab[] = [];
        for (const tab of tabs) {
          if (!this.tryLoadPreservedUrl(tab, 0)) {
            remaining.push(tab);
          }
        }
        if (remaining.length > 0) {
          originalReloadTabs.call(this.tabBrowser, remaining);
        }
      };
    }

    if (typeof this.tabBrowser.reload === "function") {
      const originalReload = this.tabBrowser.reload;
      this.originalMethods.reload = originalReload;
      this.tabBrowser.reload = () => {
        const selectedTab = this.tabBrowser.selectedTab;
        if (selectedTab && this.tryLoadPreservedUrl(selectedTab, 0)) {
          return;
        }
        originalReload.call(this.tabBrowser);
      };
    }
  }

  private uninstallReloadWrappers(): void {
    if (this.originalMethods.reloadWithFlags) {
      this.tabBrowser.reloadWithFlags = this.originalMethods.reloadWithFlags;
      delete this.originalMethods.reloadWithFlags;
    }
    if (this.originalMethods.reloadTab) {
      this.tabBrowser.reloadTab = this.originalMethods.reloadTab;
      delete this.originalMethods.reloadTab;
    }
    if (this.originalMethods.reloadTabs) {
      this.tabBrowser.reloadTabs = this.originalMethods.reloadTabs;
      delete this.originalMethods.reloadTabs;
    }
    if (this.originalMethods.reload) {
      this.tabBrowser.reload = this.originalMethods.reload;
      delete this.originalMethods.reload;
    }
  }
}

export function installTabLoadPreserveController(
  options: TabLoadPreserveControllerOptions,
  host: object = globalThis,
): TabLoadPreserveController {
  const controllerHost = host as ControllerHost;
  controllerHost[CONTROLLER_SLOT]?.destroy();
  const controller = new TabLoadPreserveController(options);
  controllerHost[CONTROLLER_SLOT] = controller;
  controller.start();
  return controller;
}

export function uninstallTabLoadPreserveController(
  controller: TabLoadPreserveController,
  host: object = globalThis,
): void {
  const controllerHost = host as ControllerHost;
  if (controllerHost[CONTROLLER_SLOT] === controller) {
    delete controllerHost[CONTROLLER_SLOT];
  }
  controller.destroy();
}
