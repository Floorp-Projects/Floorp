// SPDX-License-Identifier: MPL-2.0

import { manager } from "./index.ts";

export function ContextMenu() {
  return (
    <xul:menuitem
      data-l10n-id="status-bar"
      label="Status Bar"
      type="checkbox"
      id="toggle_statusBar"
      //https://searchfox.org/mozilla-central/rev/4d851945737082fcfb12e9e7b08156f09237aa70/browser/base/content/browser.js#4590
      toolbarId="nora-statusbar"
      checked={manager.showStatusBar()}
      onCommand={() => manager.setShowStatusBar((value) => !value)}
    />
  );
}
