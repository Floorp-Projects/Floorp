// SPDX-License-Identifier: MPL-2.0
/// <reference path="../../libs/@types/gecko/index.d.ts" />
// Read-only parent-process/window observations. No window or OS state setters.
type Addressable = { address(): unknown };
type NumberData = Addressable & { value: number };
type PointerData = { isNull(): boolean; toString(): string };
type Rectangle = Addressable & {
  left: number;
  top: number;
  right: number;
  bottom: number;
};
type MonitorData = Addressable & {
  cbSize: number;
  rcMonitor: Rectangle;
  rcWork: Rectangle;
  dwFlags: number;
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

function rectangle(value: Rectangle) {
  return {
    left: value.left,
    top: value.top,
    right: value.right,
    bottom: value.bottom,
    width: value.right - value.left,
    height: value.bottom - value.top,
  };
}

export async function captureAlertGeometry(
  phase: "before-original" | "after-original-failure",
): Promise<void> {
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
    const hr = query(state.address());
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
            flags: info.dwFlags,
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
      scope: "Owned Windows browser geometry only; no OS/window state changes",
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
      settingsChanged: false,
      limit:
        "Before/after endpoints do not observe the native ShouldShowAlert call itself. No DND/service getters run here.",
    };
    const path = PathUtils.join(output, `geometry-${phase}.json`);
    await IOUtils.writeJSON(path, snapshot);
    console.log("FLOORP_ALERT_GEOMETRY", path, JSON.stringify(snapshot));
  } finally {
    shell?.close();
    user.close();
  }
}
