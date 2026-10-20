// SPDX-License-Identifier: MPL-2.0
/// <reference path="../../libs/@types/gecko/index.d.ts" />
// Read-only observations plus bounded placement of this owned browser window.
type Addressable = { address(): unknown };
type NumberData = Addressable & { value: number };
type PointerData = { isNull(): boolean; toString(): string };
type Rectangle = Addressable & {
  left: unknown;
  top: unknown;
  right: unknown;
  bottom: unknown;
};
type MonitorData = Addressable & {
  cbSize: number;
  rcMonitor: Rectangle;
  rcWork: Rectangle;
  dwFlags: unknown;
};
type CType<T> = { (value?: unknown): T; ptr: unknown; size: number };
type NativeLibrary = {
  declare(
    name: string,
    abi: unknown,
    result: unknown,
    ...args: unknown[]
  ): (...args: unknown[]) => unknown;
  close(): void;
};
type ReadonlyCTypes = {
  winapi_abi: unknown;
  long: CType<NumberData>;
  uint32_t: CType<NumberData>;
  int32_t: CType<NumberData>;
  int: CType<NumberData>;
  voidptr_t: CType<PointerData>;
  UInt64(value: string): unknown;
  StructType<T>(name: string, fields: Record<string, unknown>[]): CType<T>;
  open(name: string): NativeLibrary;
};
type BrowserChromeWindow = Window & {
  windowState: number;
  screen: Screen & {
    left: number;
    top: number;
    availLeft: number;
    availTop: number;
  };
  docShell: {
    treeOwner: {
      QueryInterface(iid: unknown): {
        getInterface(iid: unknown): {
          QueryInterface(iid: unknown): { nativeHandle: string };
        };
      };
    };
  };
};

function safeRead<T>(read: () => T): T | { readError: string } {
  try {
    return read();
  } catch (error) {
    return { readError: String(error) };
  }
}

export function finiteNumber(value: unknown): number {
  // Actual js-ctypes scalar CData did not serialize as a JSON primitive in the
  // previous run. Numeric coercion also used by its working rect subtraction
  // reads the actual CData value; reject conversion failures/non-finite values.
  if (
    typeof value !== "number" && (typeof value !== "object" || value === null)
  ) {
    throw Error("Actual native scalar must be a number or CData object");
  }
  const result = Number(value);
  if (!Number.isFinite(result)) throw Error("Non-finite actual native scalar");
  return result;
}

function rectangle(value: Rectangle) {
  const left = finiteNumber(value.left);
  const top = finiteNumber(value.top);
  const right = finiteNumber(value.right);
  const bottom = finiteNumber(value.bottom);
  return {
    left,
    top,
    right,
    bottom,
    width: right - left,
    height: bottom - top,
  };
}

let placementOperationsAttempted = false;

export async function captureAlertGeometry(
  phase: "before-placement" | "after-placement" | "after-original-failure",
) {
  const output = Services.env.get("FLOORP_ALERT_GEOMETRY_OUTPUT");
  if (!output || Services.appinfo.OS !== "WINNT") {
    throw Error("Owned actual Windows geometry output required");
  }
  const { ctypes } = ChromeUtils.importESModule(
    "resource://gre/modules/ctypes.sys.mjs",
  ) as unknown as { ctypes: ReadonlyCTypes };
  const user = ctypes.open("user32.dll");
  let shell: NativeLibrary | undefined;
  try {
    shell = ctypes.open("shell32.dll");
    const query = shell.declare(
      "SHQueryUserNotificationState",
      ctypes.winapi_abi,
      ctypes.long,
      ctypes.int.ptr,
    ) as (state: unknown) => number;
    const state = ctypes.int();
    const hr = finiteNumber(query(state.address()));
    const RECT = ctypes.StructType<Rectangle>("FloorpReadonlyRECT", [
      { left: ctypes.long },
      { top: ctypes.long },
      { right: ctypes.long },
      { bottom: ctypes.long },
    ]);
    const MONITORINFO = ctypes.StructType<MonitorData>(
      "FloorpReadonlyMONITORINFO",
      [
        { cbSize: ctypes.uint32_t },
        { rcMonitor: RECT },
        { rcWork: RECT },
        { dwFlags: ctypes.uint32_t },
      ],
    );
    if (RECT.size !== 16 || MONITORINFO.size !== 40) {
      throw Error("Unexpected actual Windows LLP64 structure layout");
    }
    const foreground = user.declare(
      "GetForegroundWindow",
      ctypes.winapi_abi,
      ctypes.voidptr_t,
    ) as () => PointerData;
    const processId = user.declare(
      "GetWindowThreadProcessId",
      ctypes.winapi_abi,
      ctypes.uint32_t,
      ctypes.voidptr_t,
      ctypes.uint32_t.ptr,
    ) as (window: unknown, pid: unknown) => number;
    const windowRect = user.declare(
      "GetWindowRect",
      ctypes.winapi_abi,
      ctypes.int,
      ctypes.voidptr_t,
      RECT.ptr,
    ) as (window: unknown, result: unknown) => number;
    const monitorFromWindow = user.declare(
      "MonitorFromWindow",
      ctypes.winapi_abi,
      ctypes.voidptr_t,
      ctypes.voidptr_t,
      ctypes.uint32_t,
    ) as (window: unknown, flags: number) => PointerData;
    const monitorInfo = user.declare(
      "GetMonitorInfoW",
      ctypes.winapi_abi,
      ctypes.int,
      ctypes.voidptr_t,
      MONITORINFO.ptr,
    ) as (monitor: unknown, result: unknown) => number;
    const isZoomed = user.declare(
      "IsZoomed",
      ctypes.winapi_abi,
      ctypes.int,
      ctypes.voidptr_t,
    ) as (window: unknown) => number;
    const isIconic = user.declare(
      "IsIconic",
      ctypes.winapi_abi,
      ctypes.int,
      ctypes.voidptr_t,
    ) as (window: unknown) => number;
    const getStyle = user.declare(
      "GetWindowLongW",
      ctypes.winapi_abi,
      ctypes.long,
      ctypes.voidptr_t,
      ctypes.int,
    ) as (window: unknown, index: number) => number;
    const describe = (hwnd: PointerData) => {
      const pid = ctypes.uint32_t();
      const threadId = processId(hwnd, pid.address());
      const common = {
        hwnd: hwnd.toString(),
        processId: pid.value,
        threadId,
        belongsToBrowserParent: pid.value === Services.appinfo.processID,
      };
      // The foreground can belong to another application. Retain its identity
      // only; inspect rectangles/styles only for this owned browser process.
      if (!common.belongsToBrowserParent) return common;
      const bounds = RECT();
      const boundsRead = windowRect(hwnd, bounds.address()) !== 0;
      const monitor = monitorFromWindow(hwnd, 2); // MONITOR_DEFAULTTONEAREST
      const info = MONITORINFO();
      info.cbSize = MONITORINFO.size;
      const monitorRead = !monitor.isNull() &&
        monitorInfo(monitor, info.address()) !== 0;
      return {
        ...common,
        boundsRead,
        bounds: boundsRead ? rectangle(bounds) : null,
        maximized: isZoomed(hwnd) !== 0,
        minimized: isIconic(hwnd) !== 0,
        style: `0x${(getStyle(hwnd, -16) >>> 0).toString(16)}`,
        monitor: monitorRead
          ? {
            handle: monitor.toString(),
            flags: finiteNumber(info.dwFlags),
            bounds: rectangle(info.rcMonitor),
            workArea: rectangle(info.rcWork),
          }
          : null,
      };
    };
    const actualForeground = foreground();
    const windows = [];
    for (const raw of Services.wm.getEnumerator("navigator:browser")) {
      const win = raw as unknown as BrowserChromeWindow;
      const doc = win.document;
      if (!doc?.nodePrincipal.isSystemPrincipal) continue;
      windows.push({
        documentURI: doc.documentURI,
        closed: win.closed,
        windowState: win.windowState,
        fullScreen: win.fullScreen,
        sizemode: doc.documentElement?.getAttribute("sizemode"),
        activeWindow: Services.focus.activeWindow === win,
        focusedWindow: Services.focus.focusedWindow === win,
        documentFocused: doc.hasFocus(),
        visibilityState: doc.visibilityState,
        bounds: {
          screenX: win.screenX,
          screenY: win.screenY,
          outerWidth: win.outerWidth,
          outerHeight: win.outerHeight,
          innerWidth: win.innerWidth,
          innerHeight: win.innerHeight,
          devicePixelRatio: win.devicePixelRatio,
        },
        screen: {
          left: win.screen.left,
          top: win.screen.top,
          width: win.screen.width,
          height: win.screen.height,
          availLeft: win.screen.availLeft,
          availTop: win.screen.availTop,
          availWidth: win.screen.availWidth,
          availHeight: win.screen.availHeight,
        },
        native: safeRead(() => {
          const app = win.docShell.treeOwner.QueryInterface(
            Ci.nsIInterfaceRequestor,
          ).getInterface(Ci.nsIAppWindow);
          if (!app.QueryInterface) {
            throw Error("Actual app-window QueryInterface unavailable");
          }
          const value = app.QueryInterface(Ci.nsIBaseWindow).nativeHandle;
          if (!/^0x[0-9a-f]+$/i.test(value)) {
            throw Error("Invalid actual native browser HWND");
          }
          return describe(ctypes.voidptr_t(ctypes.UInt64(value)));
        }),
      });
      if (windows.length >= 8) break;
    }
    const snapshot = {
      scope:
        "Owned Windows browser geometry endpoints around explicit normal placement",
      phase,
      observedAt: new Date().toISOString(),
      processId: Services.appinfo.processID,
      runtime: {
        version: Services.appinfo.version,
        buildId: Services.appinfo.appBuildID,
      },
      parentAutomation: Cu.isInAutomation,
      parentNotificationState: {
        hresult: `0x${(hr >>> 0).toString(16).padStart(8, "0")}`,
        querySucceeded: hr >= 0,
        value: state.value,
        scope: "actual parent process at this endpoint, not in-call ShowAlert",
      },
      foreground: actualForeground.isNull()
        ? null
        : safeRead(() => describe(actualForeground)),
      windows,
      osOrNotificationSettingsChanged: false,
      ownWindowPlacementAttempted: placementOperationsAttempted,
      limit:
        "Before/after endpoints do not observe the native ShouldShowAlert call itself. No DND/service getters run here.",
    };
    const path = PathUtils.join(output, `geometry-${phase}.json`);
    await IOUtils.writeJSON(path, snapshot);
    console.log("FLOORP_ALERT_GEOMETRY", path, JSON.stringify(snapshot));
    return snapshot;
  } finally {
    shell?.close();
    user.close();
  }
}

export function placementTarget(screen: {
  availLeft: number;
  availTop: number;
  availWidth: number;
  availHeight: number;
}) {
  const work = {
    left: finiteNumber(screen.availLeft),
    top: finiteNumber(screen.availTop),
    width: finiteNumber(screen.availWidth),
    height: finiteNumber(screen.availHeight),
  };
  if (work.width < 680 || work.height < 520) {
    throw Error(
      "Actual browser work area too small for bounded normal placement",
    );
  }
  const width = Math.floor(Math.min(800, work.width - 64));
  const height = Math.floor(Math.min(600, work.height - 64));
  const left = Math.round(work.left + (work.width - width) / 2);
  const top = Math.round(work.top + (work.height - height) / 2);
  return { left, top, width, height, work };
}

function windowBounds(win: BrowserChromeWindow) {
  return {
    left: win.screenX,
    top: win.screenY,
    width: win.outerWidth,
    height: win.outerHeight,
    state: win.windowState,
    sizemode: win.document?.documentElement?.getAttribute("sizemode"),
    fullScreen: win.fullScreen,
    visible: win.document?.visibilityState === "visible",
  };
}

export async function prepareOwnedNormalWindow(): Promise<void> {
  const output = Services.env.get("FLOORP_ALERT_GEOMETRY_OUTPUT");
  if (!output || Services.appinfo.OS !== "WINNT") {
    throw Error("Owned actual Windows placement output required");
  }
  const started = Date.now();
  const outcome: {
    scope: string;
    ready: boolean;
    originalTestReady: boolean;
    osOrNotificationSettingsChanged: false;
    operations: string[];
    elapsedMilliseconds?: number;
    target?: ReturnType<typeof placementTarget>;
    observed?: ReturnType<typeof windowBounds>;
    error?: string;
    failureGeometryError?: string;
  } = {
    scope: "Only the authenticated test browser's own system window geometry",
    ready: false,
    originalTestReady: false,
    osOrNotificationSettingsChanged: false,
    operations: [],
  };
  try {
    const before = await captureAlertGeometry("before-placement");
    const candidates = Array.from(
      Services.wm.getEnumerator("navigator:browser"),
    )
      .map((raw) => raw as unknown as BrowserChromeWindow)
      .filter((win) =>
        !win.closed &&
        win.document?.nodePrincipal.isSystemPrincipal &&
        win.document?.documentURI === "chrome://browser/content/browser.xhtml"
      );
    const recorded = before.windows.filter((win) =>
      !win.closed &&
      win.documentURI === "chrome://browser/content/browser.xhtml"
    );
    if (candidates.length !== 1 || recorded.length !== 1) {
      throw Error("Exactly one owned native browser window required");
    }
    const identity = recorded[0].native;
    if (
      !("belongsToBrowserParent" in identity) ||
      identity.belongsToBrowserParent !== true
    ) {
      throw Error(
        "Actual native window does not belong to this browser parent",
      );
    }
    const win = candidates[0];
    if (win.fullScreen) {
      throw Error(
        "Unexpected fullscreen state; no fullscreen setting is changed",
      );
    }
    const target = placementTarget(win.screen);
    outcome.target = target;
    const { setTimeout } = ChromeUtils.importESModule(
      "resource://gre/modules/Timer.sys.mjs",
    ) as { setTimeout(callback: () => void, delay: number): unknown };
    const wait = async (condition: () => boolean, label: string) => {
      const deadline = Date.now() + 5000;
      while (!condition()) {
        if (win.closed || Date.now() >= deadline) {
          throw Error(`Bounded actual owned window readiness failed: ${label}`);
        }
        await new Promise<void>((resolve) => setTimeout(resolve, 25));
      }
    };
    const normal = () =>
      !win.closed && !win.fullScreen &&
      win.windowState === win.STATE_NORMAL &&
      win.document?.documentElement?.getAttribute("sizemode") === "normal";
    placementOperationsAttempted = true;
    win.restore();
    outcome.operations.push("ownedWindow.restore");
    await wait(normal, "restore to normal");
    win.resizeTo(target.width, target.height);
    outcome.operations.push("ownedWindow.resizeTo");
    win.moveTo(target.left, target.top);
    outcome.operations.push("ownedWindow.moveTo");
    await wait(() => {
      const actual = windowBounds(win);
      return normal() && actual.visible &&
        Math.abs(actual.left - target.left) <= 2 &&
        Math.abs(actual.top - target.top) <= 2 &&
        Math.abs(actual.width - target.width) <= 2 &&
        Math.abs(actual.height - target.height) <= 2 &&
        actual.left >= target.work.left && actual.top >= target.work.top &&
        actual.left + actual.width <= target.work.left + target.work.width &&
        actual.top + actual.height <= target.work.top + target.work.height;
    }, "visible normal geometry inside actual work area");
    outcome.observed = windowBounds(win);
    await captureAlertGeometry("after-placement");
    outcome.ready = true;
    // The unchanged original test starts only after this helper returns.
    outcome.originalTestReady = true;
  } catch (error) {
    outcome.error = String(error);
    try {
      await captureAlertGeometry("after-placement");
    } catch (geometryError) {
      outcome.failureGeometryError = String(geometryError);
    }
    throw error;
  } finally {
    outcome.elapsedMilliseconds = Date.now() - started;
    await IOUtils.writeJSON(PathUtils.join(output, "placement.json"), outcome);
    console.log("FLOORP_ALERT_PLACEMENT", JSON.stringify(outcome));
  }
}
