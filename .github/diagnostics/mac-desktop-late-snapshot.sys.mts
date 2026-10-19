// SPDX-License-Identifier: MPL-2.0
// Separate diagnostic: only after an original desktop case has already failed.
type NativeCtypes = {
  default_abi: unknown;
  bool: unknown;
  open(path: string): {
    declare(name: string, abi: unknown, result: unknown): () => boolean;
    close(): void;
  };
};
type BrowserHandle = {
  contentTitle?: string;
  currentURI?: { spec: string };
  isConnected?: boolean;
  browsingContext?: { currentWindowGlobal?: { innerWindowId: number } };
};
type NativeView = {
  viewType?: string;
  contextId?: number;
  active?: boolean;
  unloaded?: boolean;
  xulBrowser?: BrowserHandle;
};
type NativeExtension = {
  id: string;
  principal: nsIPrincipal;
  backgroundState?: string;
  backgroundContext?: NativeView;
  views: Set<NativeView>;
};
type NativeWindow = Window & {
  PopupNotifications: {
    panel: Element & { state: string };
    getNotification(name: string, browser: unknown): unknown;
  };
};

function read<T>(fn: () => T): T | { readError: string } {
  try {
    return fn();
  } catch (error) {
    return { readError: String(error) };
  }
}

function screenCapturePreflight() {
  const { ctypes } = ChromeUtils.importESModule(
    "resource://gre/modules/ctypes.sys.mjs",
  ) as unknown as { ctypes: NativeCtypes };
  const core = ctypes.open(
    "/System/Library/Frameworks/CoreGraphics.framework/CoreGraphics",
  );
  try {
    const preflight = core.declare(
      "CGPreflightScreenCaptureAccess",
      ctypes.default_abi,
      ctypes.bool,
    );
    return {
      granted: preflight(),
      scope:
        "actual parent browser, after original failure; never requests access",
    };
  } finally {
    core.close();
  }
}

function viewState(view: NativeView) {
  return {
    viewType: view.viewType,
    contextId: view.contextId,
    active: view.active,
    unloaded: view.unloaded,
    browser: view.xulBrowser && {
      contentTitle: view.xulBrowser.contentTitle?.slice(0, 16000),
      uri: view.xulBrowser.currentURI?.spec,
      connected: view.xulBrowser.isConnected,
      innerWindowId: view.xulBrowser.browsingContext?.currentWindowGlobal
        ?.innerWindowId,
    },
  };
}

export async function captureMacDesktopFailure(
  mode: string,
  addonId: string,
  targetBrowser: unknown,
  nativeWindow: unknown,
  originalReport: unknown,
  originalError: unknown,
): Promise<void> {
  const began = Date.now();
  const { ExtensionParent } = ChromeUtils.importESModule(
    "resource://gre/modules/ExtensionParent.sys.mjs",
  ) as unknown as {
    ExtensionParent: {
      GlobalManager: { extensionMap: Map<string, NativeExtension> };
    };
  };
  const { SitePermissions } = ChromeUtils.importESModule(
    "resource:///modules/SitePermissions.sys.mjs",
  ) as unknown as {
    SitePermissions: {
      getForPrincipal(
        principal: nsIPrincipal,
        id: string,
        browser: unknown,
      ): unknown;
    };
  };
  const extension = ExtensionParent.GlobalManager.extensionMap.get(addonId);
  if (
    extension &&
    (extension.id !== addonId || extension.principal.addonId !== addonId)
  ) throw new Error("Actual desktop extension identity differs");
  const win = nativeWindow as NativeWindow;
  const browser = targetBrowser as BrowserHandle;
  const preflight = read(screenCapturePreflight);
  const windows = [];
  for (const raw of Services.wm.getEnumerator("")) {
    if (windows.length >= 12) break;
    const row = read(() => {
      const doc = raw.document;
      if (
        doc.documentURI !== "chrome://global/content/selectDialog.xhtml" &&
        doc.documentURI !== "chrome://browser/content/browser.xhtml"
      ) return undefined;
      return {
        uri: doc.documentURI,
        readyState: doc.readyState,
        visibilityState: doc.visibilityState,
        closed: raw.closed,
        title: doc.title,
        windowType: doc.documentElement?.getAttribute("windowtype"),
      };
    });
    if (row) windows.push(row);
  }
  const snapshot = {
    scope: "after-failure Mac desktop diagnostic; zero qualification credit",
    mode,
    observedAt: new Date(began).toISOString(),
    originalError: String(originalError),
    originalReport,
    screenCapturePreflight: preflight,
    extension: extension && {
      id: extension.id,
      principalAddonId: extension.principal.addonId,
      backgroundState: extension.backgroundState,
      backgroundContext: read(() =>
        extension.backgroundContext && viewState(extension.backgroundContext)
      ),
      views: [...extension.views].slice(0, 8).map((view) =>
        read(() => viewState(view))
      ),
      screenPermission: read(() =>
        SitePermissions.getForPrincipal(
          extension.principal,
          "screen",
          targetBrowser,
        )
      ),
    },
    target: read(() => ({
      title: browser.contentTitle,
      uri: browser.currentURI?.spec,
      connected: browser.isConnected,
      innerWindowId: browser.browsingContext?.currentWindowGlobal
        ?.innerWindowId,
    })),
    popup: read(() => ({
      state: win.PopupNotifications.panel.state,
      hasNativeShareNotification: Boolean(
        win.PopupNotifications.getNotification(
          "webRTC-shareDevices",
          targetBrowser,
        ),
      ),
      text: win.PopupNotifications.panel.textContent?.slice(0, 6000),
    })),
    windows,
    collectionMilliseconds: Date.now() - began,
    limitation:
      "Late state only, after the original failure. Preflight never changes or requests OS permission; no capture request, consent action, API stub, observer or context wake is added.",
  };
  const directory = Services.env.get("FLOORP_TEST_MAC_DESKTOP_LATE_OUTPUT");
  if (!directory) throw new Error("Owned Mac diagnostic directory is missing");
  await IOUtils.writeJSON(PathUtils.join(directory, `${mode}.json`), snapshot);
  console.log("FLOORP_MAC_DESKTOP_LATE_SNAPSHOT", JSON.stringify(snapshot));
}
