// SPDX-License-Identifier: MPL-2.0

import type {
  NativeWorkspaceGestureCapabilities,
  WorkspaceGestureWindow,
} from "./types.ts";

export const SIDEBAR_GESTURE_PREF =
  "floorp.workspaces.gestures.sidebar.enabled";
export const NATIVE_GESTURE_PREF = "floorp.workspaces.gestures.native.enabled";
export const NATIVE_VERIFIED_PREF =
  "floorp.workspaces.gestures.native.verified";

export function getNativeWorkspaceGestureCapabilities(
  win: Window,
): NativeWorkspaceGestureCapabilities {
  const os = Services.appinfo.OS;
  const platform = os === "Darwin"
    ? "macos"
    : os === "WINNT"
    ? "windows"
    : os === "Linux"
    ? "linux"
    : "other";
  if (platform !== "linux") return { platform, status: "unsupported" };
  const prototype = (win as WorkspaceGestureWindow).SimpleGestureEvent
    ?.prototype;
  if (
    !prototype ||
    Reflect.get(prototype, "WORKSPACE_SWIPE_PROTOCOL_VERSION") !== 1 ||
    !["fingerCount", "phase", "operationId", "deltaY"].every((field) =>
      field in prototype
    )
  ) {
    return { platform, status: "runtime-required" };
  }
  return {
    platform,
    status: Services.prefs.getBoolPref(NATIVE_VERIFIED_PREF, false)
      ? "available"
      : "unverified",
  };
}

/** Only chrome's workspace strip or vertical tab sidebar is eligible. */
export function sidebarGestureTarget(
  target: EventTarget | null,
  doc: Document,
): Element | null {
  if (
    !(target instanceof doc.defaultView!.Element) ||
    target.ownerDocument !== doc
  ) return null;
  if (
    target.closest(
      "browser, iframe, input, textarea, select, [contenteditable]:not([contenteditable=false]), [role=textbox], panel, menupopup, #panel-sidebar-box, #sidebar-box",
    )
  ) return null;
  const area = target.closest(
    "#workspaces-panel-sidebar-section, #vertical-tabs, #tabbrowser-tabs[orient=vertical]",
  );
  if (!area) return null;
  for (let node: Element | null = target; node; node = node.parentElement) {
    if (node.scrollWidth > node.clientWidth + 2) {
      const overflow = doc.defaultView?.getComputedStyle(node)?.overflowX;
      if (overflow === "auto" || overflow === "scroll") return null;
    }
    if (node === area) break;
  }
  return area;
}
