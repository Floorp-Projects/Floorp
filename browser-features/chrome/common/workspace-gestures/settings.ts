// SPDX-License-Identifier: MPL-2.0

import ModalParent from "../modal-parent/index.ts";
import type { TFormOption } from "../modal-parent/utils/type.ts";
import {
  getNativeWorkspaceGestureCapabilities,
  NATIVE_GESTURE_PREF,
  SIDEBAR_GESTURE_PREF,
} from "./policy.ts";
import i18next from "i18next";

export async function showWorkspaceGestureSettings(win: Window): Promise<void> {
  if (win.closed) return;
  const capability = getNativeWorkspaceGestureCapabilities(win);
  const available = capability.status === "available";
  const native = available &&
    Services.prefs.getBoolPref(NATIVE_GESTURE_PREF, false);
  const sidebar = Services.prefs.getBoolPref(SIDEBAR_GESTURE_PREF, false);
  const options: TFormOption[] = [
    {
      value: "off",
      label: i18next.t("workspaceGestures.off", { defaultValue: "Off" }),
    },
    {
      value: "sidebar",
      label: i18next.t("workspaceGestures.sidebar", {
        defaultValue:
          "Horizontal swipe in the workspace or vertical tab sidebar",
      }),
    },
  ];
  if (available) {
    options.push({
      value: "native",
      label: i18next.t("workspaceGestures.native", {
        defaultValue: "Native three-finger swipe (experimental)",
      }),
    });
  }
  const status = i18next.t(`workspaceGestures.status.${capability.status}`, {
    defaultValue: capability.status === "available"
      ? "Native three-finger input is available in this verified environment."
      : capability.status === "unverified"
      ? "Native input is implemented but this environment has not been verified."
      : capability.status === "runtime-required"
      ? "Native input needs a rebuilt runtime and device verification."
      : "Direct three-finger input is not available on this platform.",
  });
  const mapping = i18next.t(
    `workspaceGestures.osMapping.${capability.platform}`,
    {
      defaultValue: capability.platform === "windows"
        ? "On Windows, compatible precision-touchpad settings can map a three-finger gesture to a custom keyboard shortcut. Availability depends on the touchpad driver."
        : capability.platform === "macos"
        ? "On macOS, reserve system trackpad gestures in System Settings first. A gesture-to-key utility is needed to send a custom Floorp shortcut."
        : capability.platform === "linux"
        ? "On Linux, use a desktop or gesture tool that can send a custom keyboard shortcut. Wayland support depends on the desktop environment."
        : "Use a gesture-to-key tool supported by your operating system to send a custom Floorp shortcut.",
    },
  );
  const parent = ModalParent.getInstance();
  parent.init();
  const result = await parent.showNoraModal(
    {
      title: i18next.t("workspaceGestures.settings", {
        defaultValue: "Workspace switching",
      }),
      description: `${status} ${mapping} ${
        i18next.t("workspaceGestures.settingsDescription", {
          defaultValue:
            "Sidebar input and native input are mutually exclusive. Page scrolling and history gestures keep their normal behavior. Sidebar sensitivity and inertia suppression depend on the device. To use an OS gesture-to-key mapping, assign the next/previous workspace actions that stop at the ends, and configure one key press per gesture.",
        })
      }`,
      forms: [{
        id: "mode",
        type: "select",
        required: true,
        label: i18next.t("workspaceGestures.input", {
          defaultValue: "Switching input",
        }),
        value: native ? "native" : sidebar ? "sidebar" : "off",
        options,
      }],
      submitLabel: i18next.t("workspaces.modal.save", { defaultValue: "Save" }),
      cancelLabel: i18next.t("workspaces.modal.cancel", {
        defaultValue: "Cancel",
      }),
    },
    { width: 560, height: 520 },
    () => {},
  );
  if (!result || win.closed) return;
  const mode = result.mode;
  if (mode !== "off" && mode !== "sidebar" && mode !== "native") return;
  // Recheck after the dialog: verification can change in another window.
  const enableNative = mode === "native" &&
    getNativeWorkspaceGestureCapabilities(win).status === "available";
  Services.prefs.setBoolPref(NATIVE_GESTURE_PREF, false);
  Services.prefs.setBoolPref(SIDEBAR_GESTURE_PREF, false);
  if (enableNative) Services.prefs.setBoolPref(NATIVE_GESTURE_PREF, true);
  else if (mode === "sidebar") {
    Services.prefs.setBoolPref(SIDEBAR_GESTURE_PREF, true);
  }
}
