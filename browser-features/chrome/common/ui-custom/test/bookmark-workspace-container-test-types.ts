// SPDX-License-Identifier: MPL-2.0

export type BookmarkBrowserWindow = Window & {
  gBrowser: GBrowser;
  gBrowserInit?: { delayedStartupFinished: boolean };
};
