/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panel } from "../../core/utils/type.ts";

export function WebSiteBrowser({ id, type, url, userContextId }: Panel) {
  return (
    <xul:browser
      id={`sidebar-panel-${id}`}
      class="sidebar-panel-browser"
      flex="1"
      xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul"
      disablehistory="true"
      disablefullscreen="true"
      tooltip="aHTMLTooltip"
      autoscroll="false"
      disableglobalhistory="true"
      messagemanagergroup="browsers"
      autocompletepopup="PopupAutoComplete"
      initialBrowsingContextGroupId="40"
      usercontextid={`${userContextId ?? 0}`}
      src={`chrome://browser/content/browser.xhtml?floorpWebPanelId=${id}`}
    />
  );
}
