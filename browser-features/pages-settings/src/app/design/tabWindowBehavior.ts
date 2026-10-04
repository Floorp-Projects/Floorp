import { rpc } from "../../lib/rpc/rpc.ts";

export const OPEN_NEW_WINDOW_PREF = "browser.link.open_newwindow";
export const TASKBAR_PREVIEWS_PREF = "browser.taskbar.previews.enable";

export const DEFAULT_OPEN_NEW_WINDOW = 3;

export type OpenNewWindowValue = 1 | 2 | 3;

export interface TabWindowBehaviorSettings {
  openNewWindow: OpenNewWindowValue;
  taskbarPreviews: boolean | null;
}

export function normalizeOpenNewWindowValue(
  value: number | null,
): OpenNewWindowValue {
  if (value === 1 || value === 2 || value === 3) {
    return value;
  }
  return DEFAULT_OPEN_NEW_WINDOW;
}

export async function getTabWindowBehaviorSettings(): Promise<
  TabWindowBehaviorSettings
> {
  const [openNewWindow, taskbarPreviews] = await Promise.all([
    rpc.getIntPref(OPEN_NEW_WINDOW_PREF),
    rpc.getBoolPref(TASKBAR_PREVIEWS_PREF),
  ]);

  return {
    openNewWindow: normalizeOpenNewWindowValue(openNewWindow),
    // null means that this platform does not expose the Windows-only pref.
    taskbarPreviews,
  };
}

export async function setOpenNewWindow(
  value: OpenNewWindowValue,
): Promise<void> {
  await rpc.setIntPref(OPEN_NEW_WINDOW_PREF, value);
}

export async function setTaskbarPreviews(enabled: boolean): Promise<void> {
  await rpc.setBoolPref(TASKBAR_PREVIEWS_PREF, enabled);
}
