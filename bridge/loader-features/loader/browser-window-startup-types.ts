// SPDX-License-Identifier: MPL-2.0

export interface BrowserStartupWindow
  extends Pick<Window, "closed" | "addEventListener" | "removeEventListener"> {
  gBrowserInit?: { delayedStartupFinished?: boolean };
}
