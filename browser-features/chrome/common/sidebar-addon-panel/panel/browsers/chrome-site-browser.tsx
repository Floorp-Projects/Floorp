/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import type { Panel } from "../../core/utils/type.ts";
import { STATIC_PANEL_DATA } from "../../core/static-panels.ts";

export function ChromeSiteBrowser({ id, type, url }: Panel) {
  const panel = STATIC_PANEL_DATA[url as keyof typeof STATIC_PANEL_DATA];
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
      {...(panel.url?.startsWith("http") && import.meta.env.MODE === "dev"
        ? {
            type: "content",
            remote: "true",
            maychangeremoteness: "true",
          }
        : {})}
      src={panel.url}
    />
  );
}
