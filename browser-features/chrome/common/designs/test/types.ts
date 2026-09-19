// SPDX-License-Identifier: MPL-2.0

export interface AppearanceTestBrowser extends GBrowser {
  addTabGroup(tabs: XULElement[], options: { label: string }): XULElement;
}
