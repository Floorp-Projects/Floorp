/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { showStatusbar, setShowStatusbar } from "./statusbar-manager";

export function ContextMenu() {
  return (
    <xul:menuitem
      data-l10n-id="status-bar"
      label="Status Bar"
      type="checkbox"
      id="toggle_statusBar"
      //https://searchfox.org/mozilla-central/rev/4d851945737082fcfb12e9e7b08156f09237aa70/browser/base/content/browser.js#4590
      toolbarId="nora-statusbar"
      checked={showStatusbar()}
      onCommand={() => setShowStatusbar((value) => !value)}
    />
  );
}
