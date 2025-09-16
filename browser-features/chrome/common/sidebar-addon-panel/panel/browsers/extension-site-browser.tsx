/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panel } from "../../core/utils/type.ts";

export function ExtensionSiteBrowser({ id, type, url }: Panel) {
  return (
    <xul:browser
      id={`sidebar-panel-${id}`}
      class="sidebar-panel-browser"
      flex="1"
      disablehistory="true"
      autoscroll="false"
      disablefullscreen="true"
      src="chrome://browser/content/webext-panels.xhtml"
    />
  );
}
