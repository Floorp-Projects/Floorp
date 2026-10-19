// SPDX-License-Identifier: MPL-2.0
// Diagnostic only: invoked once after the original active-alert wait rejects.
type CData = { value: number; address(): unknown };
type CType = { (): CData; ptr: unknown };
type CTypes = {
  winapi_abi: unknown;
  long: unknown;
  int: CType;
  open(name: string): {
    declare(
      name: string,
      abi: unknown,
      result: unknown,
      arg: unknown,
    ): (state: unknown) => number;
    close(): void;
  };
};
type ImageHandle = Element & {
  QueryInterface(iid: unknown): {
    getRequest(kind: number): {
      imageStatus: number;
      image: { width: number; height: number } | null;
      URI: { spec: string } | null;
    } | null;
  };
};
type NativeAlert = Window & {
  gIsActive?: boolean;
  gRequireInteraction?: boolean;
  gAlertCallbacks?: unknown;
  arguments?: unknown[];
};

function read<T>(fn: () => T): T | { readError: string } {
  try {
    return fn();
  } catch (error) {
    return { readError: String(error) };
  }
}

function notificationState() {
  const { ctypes } = ChromeUtils.importESModule(
    "resource://gre/modules/ctypes.sys.mjs",
  ) as unknown as { ctypes: CTypes };
  const shell = ctypes.open("shell32.dll");
  try {
    const query = shell.declare(
      "SHQueryUserNotificationState",
      ctypes.winapi_abi,
      ctypes.long,
      ctypes.int.ptr,
    );
    const state = ctypes.int();
    const result = query(state.address());
    return {
      hresult: `0x${(result >>> 0).toString(16).padStart(8, "0")}`,
      querySucceeded: result >= 0,
      value: state.value,
      scope: "actual parent browser process, after original timeout",
    };
  } finally {
    shell.close();
  }
}

function existingAlertService() {
  const contract = "@mozilla.org/alerts-service;1";
  if (!Components.manager.QueryInterface) {
    throw new Error("Native service-manager QueryInterface unavailable");
  }
  const manager = Components.manager.QueryInterface(Ci.nsIServiceManager);
  const instantiated = manager.isServiceInstantiatedByContractID(
    contract,
    Ci.nsIAlertsService,
  );
  if (!instantiated) return { instantiated, serviceRead: false };
  // The native instance already exists. No new alerts service is constructed.
  const service = Cc[contract].getService(Ci.nsIAlertsService);
  return {
    instantiated,
    serviceRead: true,
    dndGetterLimit:
      "Native DND getters may lazily obtain the XUL backend singleton. Window metadata and parent OS query are captured before these getters; no setter or alert-creation method is invoked.",
    dnd: read(() => {
      if (!service.QueryInterface) {
        throw new Error("Existing alerts-service QueryInterface unavailable");
      }
      const dnd = service.QueryInterface(Ci.nsIAlertsDoNotDisturb);
      return {
        manualDoNotDisturb: dnd.manualDoNotDisturb,
        suppressForScreenSharing: dnd.suppressForScreenSharing,
      };
    }),
  };
}

function alertWindow(win: NativeAlert) {
  const doc = win.document;
  const image = doc.querySelector("#alertImage") as ImageHandle | null;
  return {
    documentURI: doc.documentURI,
    readyState: doc.readyState,
    visibilityState: doc.visibilityState,
    hidden: doc.hidden,
    closed: win.closed,
    windowType: doc.documentElement?.getAttribute("windowtype"),
    title: doc.querySelector("#alertTitleLabel")?.getAttribute("value"),
    text: doc.querySelector("#alertTextLabel")?.textContent?.slice(0, 2000),
    gIsActive: win.gIsActive,
    gRequireInteraction: win.gRequireInteraction,
    hasCallbacks: Boolean(win.gAlertCallbacks),
    argumentCount: win.arguments?.length,
    bounds: {
      screenX: win.screenX,
      screenY: win.screenY,
      width: win.outerWidth,
      height: win.outerHeight,
    },
    image: image && {
      src: image.getAttribute("src")?.slice(0, 512),
      hidden: image.getAttribute("hidden"),
      request: read(() => {
        const current = Ci.nsIImageLoadingContent.CURRENT_REQUEST;
        if (current === undefined) {
          throw new Error("Native image CURRENT_REQUEST constant unavailable");
        }
        const request = image.QueryInterface(Ci.nsIImageLoadingContent)
          .getRequest(current);
        return request && {
          imageStatus: request.imageStatus,
          uri: request.URI?.spec.slice(0, 512),
          width: request.image?.width,
          height: request.image?.height,
        };
      }),
    },
  };
}

export async function captureLateAlertFailure(
  title: string,
  originalError: unknown,
  fixtureState: unknown,
): Promise<void> {
  const began = Date.now();
  const windows = [];
  let matchingWindows = 0;
  // The native IDL allows null for all windows; local generated types omit it.
  for (const raw of Services.wm.getEnumerator(null as unknown as string)) {
    const win = raw as unknown as NativeAlert;
    if (
      win.document.documentURI !==
        "chrome://global/content/alerts/alert.xhtml" ||
      !win.document.nodePrincipal.isSystemPrincipal
    ) continue;
    matchingWindows++;
    if (windows.length < 8) windows.push(read(() => alertWindow(win)));
  }
  const snapshot = {
    scope: "failure-only read-only diagnostic; zero qualification credit",
    observedAt: new Date(began).toISOString(),
    title,
    originalError: String(originalError),
    originalPredicate: "alert:alert window, exact title and gIsActive",
    fixtureState,
    parentAutomation: read(() => Cu.isInAutomation),
    parentNotificationState: read(notificationState),
    existingAlertService: read(existingAlertService),
    preferences: {
      useSystemBackend: Services.prefs.getBoolPref("alerts.useSystemBackend"),
      requireInteraction: Services.prefs.getBoolPref(
        "dom.webnotifications.requireinteraction.enabled",
        false,
      ),
      persistentAlertLimit: Services.prefs.getIntPref(
        "dom.webnotifications.requireinteraction.count",
        -1,
      ),
    },
    matchingWindows,
    windows,
    imageStageLimit:
      "Existing alert-window image requests only. The extension's earlier async fetchDecodedImage promise is not directly observable. No new decode, fetch, alert, observer or warmup is performed.",
    collectionMilliseconds: Date.now() - began,
  };
  const output = Services.env.get("FLOORP_TEST_ALERT_LATE_SNAPSHOT_OUTPUT");
  if (!output) throw new Error("Owned diagnostic sidecar path is missing");
  await IOUtils.writeJSON(output, snapshot);
  console.log(
    `FLOORP_ALERT_LATE_SNAPSHOT ${output} ${JSON.stringify(snapshot)}`,
  );
}
